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

#include <winscard.h>

#include "remote-control-stub.h"
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

static void reconnect(struct smartcard *smartcard)
{
	LONG rv = SCardReconnect(smartcard->card, SCARD_SHARE_SHARED,
			SCARD_PROTOCOL_ANY, SCARD_LEAVE_CARD,
			&smartcard->protocol);
	if (rv != SCARD_S_SUCCESS)
		smartcard->protocol = 0;

	if (!ensure_buffer(smartcard, smartcard->state.cbAtr)) {
		memcpy(smartcard->rcv_buffer, smartcard->state.rgbAtr,
				smartcard->state.cbAtr);
		smartcard->rcv_len = smartcard->state.cbAtr;

		pr_debug_atr("Atr", smartcard->rcv_buffer, smartcard->rcv_len);
	}
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
		struct rpc_server *server, GKeyFile *config)
{
	struct remote_control *rc = rpc_server_priv(server);
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

	rv = SCardConnect(smartcard->ctx, smartcard->device, SCARD_SHARE_DIRECT,
		SCARD_PROTOCOL_ANY, &smartcard->card, &smartcard->protocol);
	if (rv != SCARD_S_SUCCESS) {
		pr_debug("SCardConnect: %s", pcsc_stringify_error(rv));
		pr_debug_readers(smartcard->ctx);
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
	default:
		*typep = SMARTCARD_TYPE_UNKNOWN;
	}
	return 0;
}

ssize_t smartcard_read_pcsc(struct smartcard *smartcard, off_t offset,
		void *buffer, size_t size)
{
	size_t count = size;

	if (!smartcard || !buffer || !size)
		return -EINVAL;

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
	ssize_t ret;
	LONG rv;

	if (!smartcard)
		return -EINVAL;

	switch (smartcard->protocol) {
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
		return -EIO;
	}

	smartcard->rcv_len = smartcard->rcv_size;
	rv = SCardTransmit(smartcard->card, pioSendPci, buffer, size, NULL,
			smartcard->rcv_buffer, &smartcard->rcv_len);
	switch (rv) {
	case SCARD_S_SUCCESS:
		return size;
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
	return ret;
}
