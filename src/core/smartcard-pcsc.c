/*
 * Copyright (C) 2016 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#define pr_fmt(fmt) "smartcard-pcsc: " fmt

#include <regex.h>
#include <winscard.h>

#include "remote-control.h"
#include "glogging.h"

#define STATUS_ERROR_DELAY 500000
#define INITIAL_RCV_BUFFER_SIZE 256

struct smartcard {
	struct remote_control *rc;
	SCARD_READERSTATE state;
	SCARDHANDLE  card;
	SCARDCONTEXT ctx;
	GThread *scan_thread;
	gboolean scan_done;
	gchar *device;
	DWORD protocol;
	BYTE *rcv_buffer;
	DWORD rcv_size;
	DWORD rcv_len;
};

static void pr_debug_readers(SCARDCONTEXT ctx)
{
	DWORD dwReaders = SCARD_AUTOALLOCATE;
	LPSTR mszReaders = NULL;
	char *ptr;

	if (SCardListReaders(ctx, NULL, (LPSTR)&mszReaders, &dwReaders) !=
			SCARD_S_SUCCESS)
		return;
	ptr = mszReaders;
	pr_debug("Available readers:");
	while (*ptr) {
		pr_debug("   \"%s\"", ptr);
		ptr += strlen(ptr) + 1;
	}
	(void)SCardFreeMemory(ctx, mszReaders);
}

static void pr_debug_state(gchar *prefix, DWORD state)
{
	pr_debug("%s: (0x%lx) %s%s%s%s%s%s%s%s%s%s%s%s", prefix, state,
		(state & SCARD_STATE_UNAWARE)     ? "UNAWARE "     : "",
		(state & SCARD_STATE_IGNORE)      ? "IGNORE "      : "",
		(state & SCARD_STATE_CHANGED)     ? "CHANGED "     : "",
		(state & SCARD_STATE_UNKNOWN)     ? "UNKNOWN "     : "",
		(state & SCARD_STATE_UNAVAILABLE) ? "UNAVAILABLE " : "",
		(state & SCARD_STATE_EMPTY)       ? "EMPTY "       : "",
		(state & SCARD_STATE_PRESENT)     ? "PRESENT "     : "",
		(state & SCARD_STATE_ATRMATCH)    ? "ATRMATCH "    : "",
		(state & SCARD_STATE_EXCLUSIVE)   ? "EXCLUSIVE "   : "",
		(state & SCARD_STATE_INUSE)       ? "INUSE "       : "",
		(state & SCARD_STATE_MUTE)        ? "MUTE "        : "",
		(state & SCARD_STATE_UNPOWERED)   ? "UNPOWERED "   : "");
}

static void pr_debug_atr(gchar *prefix, BYTE *atr, DWORD len)
{
	char atr_str[MAX_ATR_SIZE*3+1] = {0,};
	int i;

	if (!len)
		return;

	for (i = 0; i < len; i++)
		sprintf(&atr_str[i * 3], "%02x ", atr[i]);
	atr_str[i * 3 + 1] = 0;
	pr_debug("%s: %s", prefix, atr_str);
}

static void fire_event(struct event_manager *manager, DWORD state)
{
	struct event event;
	int err;

	event.source = EVENT_SOURCE_SMARTCARD;
	event.smartcard.state = (state &  SCARD_STATE_PRESENT) ?
		 EVENT_SMARTCARD_STATE_INSERTED : EVENT_SMARTCARD_STATE_REMOVED;

	err = event_manager_report(manager, &event);
	if (err < 0 && err != -EBADF)
		g_debug("event_manager_report(): %s", strerror(-err));
}

static int ensure_buffer(struct smartcard *smartcard, DWORD size)
{
	BYTE *rcv_buffer;

	smartcard->rcv_len = 0;
	if (size <= smartcard->rcv_size)
		return 0;

	rcv_buffer = malloc(size);
	if (!rcv_buffer)
		return -ENOMEM;

	free(smartcard->rcv_buffer);
	smartcard->rcv_buffer = rcv_buffer;
	smartcard->rcv_size = size;

	return 0;
}

static void search_reader(struct smartcard *smartcard) {
	DWORD dwReaders = SCARD_AUTOALLOCATE;
	LPSTR mszReaders = NULL;
	regex_t regex;
	char *ptr;
	LONG rv;

	rv = SCardListReaders(smartcard->ctx, NULL, (LPSTR)&mszReaders,
			&dwReaders);
	if (rv != SCARD_S_SUCCESS) {
		pr_debug("SCardListReaders: %s", pcsc_stringify_error(rv));
		return;
	}

	if (!smartcard->device || regcomp(&regex, smartcard->device, 0)) {
		pr_debug_readers(smartcard->ctx);
		return;
	}

	ptr = mszReaders;
	pr_debug("Search for reader: \"%s\"", smartcard->device);
	while (*ptr) {
		if (!regexec(&regex, ptr, 0, NULL, 0)) {
			free(smartcard->device);
			smartcard->device = strdup(ptr);
			pr_debug("   \"%s\" <- match", ptr);
		} else {
			pr_debug("   \"%s\"", ptr);
		}
		ptr += strlen(ptr) + 1;
	}
	regfree(&regex);
	(void)SCardFreeMemory(smartcard->ctx, mszReaders);
}

static void reconnect(struct smartcard *smartcard)
{
	LONG rv = SCardReconnect(smartcard->card, SCARD_SHARE_SHARED,
			SCARD_PROTOCOL_ANY, SCARD_LEAVE_CARD,
			&smartcard->protocol);
	if (rv != SCARD_S_SUCCESS)
		smartcard->protocol = 0;

	if (smartcard->state.cbAtr == 2 &&
			smartcard->state.rgbAtr[0] == 0x3b &&
			smartcard->state.rgbAtr[1] == 0)
		smartcard->protocol = SCARD_PROTOCOL_RAW;

	if (!ensure_buffer(smartcard, smartcard->state.cbAtr)) {
		memcpy(smartcard->rcv_buffer, smartcard->state.rgbAtr,
				smartcard->state.cbAtr);
		smartcard->rcv_len = smartcard->state.cbAtr;

		pr_debug_atr("Atr", smartcard->rcv_buffer, smartcard->rcv_len);
	}
}

#define RESPONSE_SIZE 2
unsigned char RESPONSE_SUCCESS[RESPONSE_SIZE] = {0x90, 0x00};
unsigned char RESPONSE_UNSUCCESSFUL[RESPONSE_SIZE] = {0x64, 0x00};
unsigned char RESPONSE_WARNING_NO_CARD[RESPONSE_SIZE] = {0x62, 0x00};

static ssize_t process_icc_command(struct smartcard *smartcard,
		const unsigned char *buffer, size_t size, SCARDHANDLE card)
{
	DWORD protocol;
	LONG rv;
	int ret;

	if (size < 2)
		return -EINVAL;

	ret = ensure_buffer(smartcard, RESPONSE_SIZE);
	if (ret)
		return ret;

	smartcard->rcv_len = sizeof(RESPONSE_UNSUCCESSFUL);
	memcpy(smartcard->rcv_buffer, RESPONSE_UNSUCCESSFUL,
			sizeof(RESPONSE_UNSUCCESSFUL));

	switch (buffer[1]) {
	case 0x11: /* Reset CT */
		rv = SCardReconnect(card, SCARD_SHARE_SHARED,
			SCARD_PROTOCOL_ANY, SCARD_RESET_CARD, &protocol);
		if (rv == SCARD_S_SUCCESS)
			memcpy(smartcard->rcv_buffer,  RESPONSE_SUCCESS,
					sizeof(RESPONSE_SUCCESS));
		break;
	case 0x12: /* Request ICC */
		if (!smartcard->state.cbAtr) {
			memcpy(smartcard->rcv_buffer,
					RESPONSE_WARNING_NO_CARD,
					sizeof(RESPONSE_WARNING_NO_CARD));
			break;
		}
		smartcard->rcv_len = 0;
		if (size > 3 && buffer[3] == 1) {
			if (ensure_buffer(smartcard, smartcard->state.cbAtr +
					sizeof(RESPONSE_SUCCESS)))
				break;
			memcpy(smartcard->rcv_buffer, smartcard->state.rgbAtr,
					smartcard->state.cbAtr);
			smartcard->rcv_len = smartcard->state.cbAtr;
		}
		memcpy(&smartcard->rcv_buffer[smartcard->rcv_len],
				RESPONSE_SUCCESS, sizeof(RESPONSE_SUCCESS));
		smartcard->rcv_len += sizeof(RESPONSE_SUCCESS);
		break;
	case 0x15: /* Eject ICC */
		rv = SCardReconnect(smartcard->card, SCARD_SHARE_SHARED,
			SCARD_PROTOCOL_ANY, SCARD_EJECT_CARD, &protocol);
		if (rv == SCARD_S_SUCCESS)
			memcpy(smartcard->rcv_buffer, RESPONSE_SUCCESS,
					sizeof( RESPONSE_SUCCESS));
		break;
	default:
		break;
	}
	return size;
}

static gpointer scan_thread(gpointer data)
{
	struct smartcard *smartcard = data;
	struct event_manager *manager;

	manager = remote_control_get_event_manager(smartcard->rc);

	smartcard->state.szReader = smartcard->device;
	smartcard->state.dwCurrentState = SCARD_STATE_UNAWARE;

	while (!smartcard->scan_done) {
		if (SCardGetStatusChange(smartcard->ctx, INFINITE,
				&smartcard->state, 1) != SCARD_S_SUCCESS) {
			g_usleep(STATUS_ERROR_DELAY);
			continue;
		}
		pr_debug_state("New state", smartcard->state.dwEventState);
		if (smartcard->state.dwEventState & SCARD_STATE_CHANGED) {
			smartcard->state.dwCurrentState =
					smartcard->state.dwEventState;
			fire_event(manager, smartcard->state.dwCurrentState);
			reconnect(smartcard);
		}
	}
	return NULL;
}

int smartcard_free_pcsc(struct smartcard *smartcard)
{
	if (!smartcard)
		return -EINVAL;

	smartcard->scan_done = TRUE;
	(void)SCardCancel(smartcard->ctx);
	g_thread_join(smartcard->scan_thread);

	(void)SCardReleaseContext(smartcard->ctx);
	free(smartcard->device);
	free(smartcard->rcv_buffer);

	free(smartcard);
	return 0;
}

int smartcard_create_pcsc(struct smartcard **smartcardp,
		struct remote_control *rc, GKeyFile *config)
{
	struct smartcard *smartcard;
	LONG rv;

	if (!smartcardp)
		return -EINVAL;

	if (!g_key_file_has_group(config, "smartcard"))
		return -EIO;

	smartcard = malloc(sizeof(*smartcard));
	if (!smartcard)
		return -ENOMEM;
	memset(smartcard, 0, sizeof(*smartcard));

	smartcard->rc = rc;
	(void)ensure_buffer(smartcard, INITIAL_RCV_BUFFER_SIZE);

	smartcard->device = g_key_file_get_string(config, "smartcard", "device",
			NULL);
	if (!smartcard->device)
		goto nodevice;

	rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL,
			&smartcard->ctx);
	if (rv != SCARD_S_SUCCESS) {
		pr_debug("SCardEstablishContext: %s", pcsc_stringify_error(rv));
		goto nodevice;
	}

	search_reader(smartcard);
	rv = SCardConnect(smartcard->ctx, smartcard->device, SCARD_SHARE_DIRECT,
		SCARD_PROTOCOL_ANY, &smartcard->card, &smartcard->protocol);
	if (rv != SCARD_S_SUCCESS) {
		pr_debug("SCardConnect: %s", pcsc_stringify_error(rv));
		goto nodevice;
	}

	smartcard->scan_thread = g_thread_new("pcsc_scan", scan_thread,
			smartcard);
	if (!smartcard->scan_thread) {
		pr_debug("Failed to create scan thread");
		goto nodevice;
	}

	pr_debug("smartcard_create_pcsc: Using device %s", smartcard->device);
	*smartcardp = smartcard;
	return 0;

nodevice:
	(void)smartcard_free_pcsc(smartcard);
	return -ENODEV;
}

int smartcard_get_type_pcsc(struct smartcard *smartcard, unsigned int *typep)
{
	if (!smartcard || !typep)
		return -EINVAL;

	switch (smartcard->protocol) {
	case SCARD_PROTOCOL_T0:
		*typep = SMARTCARD_TYPE_T0;
		break;
	case SCARD_PROTOCOL_T1:
		*typep = SMARTCARD_TYPE_T1;
		break;
	case SCARD_PROTOCOL_RAW:
		*typep = SMARTCARD_TYPE_I2C;
		break;
	default:
		*typep = SMARTCARD_TYPE_UNKNOWN;
	}
	return 0;
}

/*
 * Reading memory cards using Advanced Card Systems Ltd. ACR38x (CCID)
 *
 * Currently only type 1 (1, 2, 4, 8 and 16 kilobit I2C Card) is supported
 */
static ssize_t smartcard_read_mc(struct smartcard *smartcard, off_t offset,
		void *buffer, size_t size)
{
	const SCARD_IO_REQUEST *pioSendPci = SCARD_PCI_T0;
	unsigned char type_buf[] = {0xFF, 0xA4, 0x00, 0x00, 0x01, 0x01};
	unsigned char snd_buf[] = {0xFF, 0xB0,
			(offset >> 8) & 0xFF, offset & 0xFF,
			size > 0xFF ? 0xFF : size};
	SCARDCONTEXT context;
	ssize_t ret = -EIO;
	SCARDHANDLE card;
	DWORD protocol;
	LONG rv;

	if (!smartcard)
		return -EINVAL;

	rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &context);
	if (rv != SCARD_S_SUCCESS) {
		pr_debug("%s SCardEstablishContext: %s", __FUNCTION__,
				pcsc_stringify_error(rv));
		goto cleanup;
	}
	rv = SCardConnect(context, smartcard->device, SCARD_SHARE_SHARED,
			SCARD_PROTOCOL_T0, &card, &protocol);
	if (rv != SCARD_S_SUCCESS) {
		pr_debug("%s SCardEstablishContext: %s", __FUNCTION__,
				pcsc_stringify_error(rv));
		goto cleanup;
	}

	smartcard->rcv_len = smartcard->rcv_size;
	rv = SCardTransmit(card, pioSendPci,
			type_buf, sizeof(type_buf), NULL,
			smartcard->rcv_buffer, &smartcard->rcv_len);
	if (rv < 0) {
		pr_debug("%s SCardTransmit set memory card: %s", __FUNCTION__,
				pcsc_stringify_error(rv));
		goto cleanup;
	}
	if (smartcard->rcv_len < 2 ||
		smartcard->rcv_buffer[smartcard->rcv_len - 2] != 0x90 ||
		smartcard->rcv_buffer[smartcard->rcv_len - 1] != 0) {

		pr_debug("%s set memory card: %s", __FUNCTION__,
				pcsc_stringify_error(rv));
		goto cleanup;
	}

	rv = ensure_buffer(smartcard, size + 2);
	if (rv < 0)
		return rv;

	smartcard->rcv_len = smartcard->rcv_size;
	rv = SCardTransmit(card, pioSendPci,
			snd_buf, sizeof(snd_buf), NULL,
			smartcard->rcv_buffer, &smartcard->rcv_len);

	switch (rv) {
	case SCARD_S_SUCCESS:
		if (smartcard->rcv_len < 2 ||
			smartcard->rcv_buffer[smartcard->rcv_len - 2] != 0x90 ||
			smartcard->rcv_buffer[smartcard->rcv_len - 1] != 0) {

			pr_debug("%s SCardTransmit: %ld %02x%02x",
				__FUNCTION__, smartcard->rcv_len,
				smartcard->rcv_buffer[smartcard->rcv_len - 2],
				smartcard->rcv_buffer[smartcard->rcv_len - 1]);
			ret = -EIO;
			break;
		}
		ret = smartcard->rcv_len -2;
		goto cleanup;
	case SCARD_W_RESET_CARD:
		reconnect(smartcard);
		ret = -EAGAIN;
		break;
	case SCARD_E_INSUFFICIENT_BUFFER:
		(void)ensure_buffer(smartcard, smartcard->rcv_len);
		ret = -EAGAIN;
		break;
	case SCARD_E_INVALID_HANDLE:
	case SCARD_E_NO_SERVICE:
	case SCARD_E_READER_UNAVAILABLE:
	case SCARD_W_REMOVED_CARD:
		ret = -EBADF;
		break;
	case SCARD_E_INVALID_VALUE:
	case SCARD_E_INVALID_PARAMETER:
		ret = -EINVAL;
		break;
	case SCARD_E_NOT_TRANSACTED:
	case SCARD_E_PROTO_MISMATCH:
	case SCARD_F_COMM_ERROR:
	default:
		ret = -EIO;
	}

	pr_debug("%s SCardTransmit: %s len %ld",  __FUNCTION__,
			pcsc_stringify_error(rv), smartcard->rcv_len);

	smartcard->rcv_len = 0;
cleanup:
	if (ret > 0)
		memcpy(buffer, smartcard->rcv_buffer, ret);
	(void)SCardDisconnect(card, SCARD_LEAVE_CARD);
	(void)SCardReleaseContext(context);
	return ret;
}

ssize_t smartcard_read_pcsc(struct smartcard *smartcard, off_t offset,
		void *buffer, size_t size)
{
	size_t count = size;

	if (!smartcard || !buffer || !size)
		return -EINVAL;

	if (smartcard->protocol == SCARD_PROTOCOL_RAW)
		return smartcard_read_mc(smartcard, offset, buffer, size);

	if (offset >= smartcard->rcv_len)
		return 0;
	if (offset + count > smartcard->rcv_len)
		count = smartcard->rcv_len - offset;
	memcpy(buffer, smartcard->rcv_buffer, count);
	return count;
}

ssize_t smartcard_write_pcsc(struct smartcard *smartcard, off_t offset,
		const void *buffer, size_t size)
{
	const SCARD_IO_REQUEST *pioSendPci;
	SCARDCONTEXT context;
	ssize_t ret = -EIO;
	SCARDHANDLE card;
	DWORD protocol;
	LONG rv;

	if (!smartcard)
		return -EINVAL;

	rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &context);
	if (rv != SCARD_S_SUCCESS) {
		pr_debug("%s SCardEstablishContext: %s", __FUNCTION__,
				pcsc_stringify_error(rv));
		goto cleanup;
	}
	rv = SCardConnect(context, smartcard->device, SCARD_SHARE_SHARED,
			SCARD_PROTOCOL_ANY, &card, &protocol);
	if (rv != SCARD_S_SUCCESS) {
		pr_debug("%s SCardEstablishContext: %s", __FUNCTION__,
				pcsc_stringify_error(rv));
		goto cleanup;
	}

	if (size && ((unsigned char *)buffer)[0] == 0x20)
		return process_icc_command(smartcard, buffer, size, card);

	switch (protocol) {
	case SCARD_PROTOCOL_T0:
		pioSendPci = SCARD_PCI_T0;
		break;
	case SCARD_PROTOCOL_T1:
		pioSendPci = SCARD_PCI_T1;
		break;
	case SCARD_PROTOCOL_RAW:
		pioSendPci = SCARD_PCI_RAW;
		break;
	default:
		/* No valid protocol */
		goto cleanup;
	}

	smartcard->rcv_len = smartcard->rcv_size;
	rv = SCardTransmit(card, pioSendPci, buffer, size, NULL,
			smartcard->rcv_buffer, &smartcard->rcv_len);
	switch (rv) {
	case SCARD_S_SUCCESS:
		ret = size;
		goto cleanup;
	case SCARD_W_RESET_CARD:
		reconnect(smartcard);
		ret = -EAGAIN;
		break;
	case SCARD_E_INSUFFICIENT_BUFFER:
		(void)ensure_buffer(smartcard, smartcard->rcv_len);
		ret = -EAGAIN;
		break;
	case SCARD_E_INVALID_HANDLE:
	case SCARD_E_NO_SERVICE:
	case SCARD_E_READER_UNAVAILABLE:
	case SCARD_W_REMOVED_CARD:
		ret = -EBADF;
		break;
	case SCARD_E_INVALID_VALUE:
	case SCARD_E_INVALID_PARAMETER:
		ret = -EINVAL;
		break;
	case SCARD_E_NOT_TRANSACTED:
	case SCARD_E_PROTO_MISMATCH:
	case SCARD_F_COMM_ERROR:
	default:
		ret = -EIO;
	}
	pr_debug("SCardTransmit: %s len %ld", pcsc_stringify_error(rv),
			smartcard->rcv_len);
	smartcard->rcv_len = 0;
cleanup:
	(void)SCardDisconnect(card, SCARD_LEAVE_CARD);
	(void)SCardReleaseContext(context);
	return ret;
}
