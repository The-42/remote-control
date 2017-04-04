/*
 * Copyright (C) 2016 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * See http://www.nxp.com/documents/application_note/AN10207.pdf for
 * documentation of the protocol.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#define pr_fmt(fmt) "smartcard-nxp: " fmt

#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>

#include "remote-control.h"
#include "glogging.h"

#define SC_NXP_FALLBACK_DEVICE		"/dev/ttyUSB0"

#define ALPAR_ACK 0x60
#define ALPAR_NACK 0xE0
#define ALPAR_MAX_PAYLOAD 506
#define ALPAR_HEADER_LEN 4
#define ALPAR_MAX_BUFFER (ALPAR_HEADER_LEN + ALPAR_MAX_PAYLOAD + 1)
#define ALPAR_WAIT 1000
#define NXP_SCAN_WAIT 100

#define NXP_CARD_COMMAND 0x00
#define NXP_CHECK_PRES_CARD 0x09
#define NXP_SET_CARD_BAUD_RATE 0x0B
#define NXP_READ_I2C 0x12
#define NXP_READ_I2C_EXTENDED 0x13
#define NXP_POWER_UP_ISO 0x69
#define NXP_POWER_UP_I2C 0x6C
#define NXP_GET_CARD_PARAM 0xA6

const unsigned char NXP_EGK_SUCCESS[] = { 0x90, 0x00 };
const unsigned char NXP_EGK_REQUEST_ICC[] = { 0x20, 0x12, 0x01, 0x1, 0x01, 0x01 };

struct smartcard {
	struct remote_control *rc;
	GThread *scan_thread;
	gboolean scan_done;
	GMutex serial_mutex;
	gchar *device;
	int fd;
	struct termios sct;
	unsigned int type;
	uint8_t rcv_buffer[ALPAR_MAX_BUFFER];
	ssize_t rcv_len;
	uint8_t atr_buffer[ALPAR_MAX_BUFFER];
	ssize_t atr_len;
};

static void hexdump(char *msg, uint8_t *buf, size_t len)
{
	const size_t rowsize = 16;
	const uint8_t *ptr = buf;
	FILE *fp = stdout;
	size_t i, j;

	fprintf(fp, "%s\n", msg);
	for (j = 0; j < len; j += rowsize) {

		for (i = 0; i < rowsize; i++) {
			if ((j + i) < len)
				fprintf(fp, "%s%02x", i ? " " : "", ptr[j + i]);
			else
				fprintf(fp, "%s  ", i ? " " : "");
		}

		fprintf(fp, " | ");

		for (i = 0; i < rowsize; i++) {
			if ((j + i) < len) {
				if (isprint(ptr[j + i]))
					fprintf(fp, "%c", ptr[j + i]);
				else
					fprintf(fp, ".");
			} else {
				fprintf(fp, " ");
			}
		}

		fprintf(fp, " |\n");
	}
}

static uint8_t *sc_payload(uint8_t *buf)
{
	return buf + ALPAR_HEADER_LEN;
}

static int sc_is_card_action(uint8_t *buf, int payload_len)
{
	return (buf[0] == ALPAR_ACK && payload_len == 1 && buf[3] == 0xA0);
}

static int sc_is_wake_up(uint8_t *buf, int payload_len)
{
	return (buf[0] == ALPAR_ACK && buf[3] == 0xBB &&
			payload_len == 1 && buf[4] == 0x01);
}

static int sc_is_bad_fidi(uint8_t * buf)
{
	return (buf[0] == ALPAR_NACK && buf[1] == 0 && buf[2] == 1 &&
			buf[4] == 0x86);
}

static const char* sc_err_str(uint8_t err)
{
	switch (err) {
		case 0x08: return "Length of the data buffer too short";

		case 0x20: return "Wrong APDU";
		case 0x21: return "Too short APDU";
		case 0x22: return "Card mute now (during T=1 exchange)";
		case 0x24: return "Bad NAD";
		case 0x26: return "Resynchronized";
		case 0x27: return "Chain aborted";
		case 0x29: return "Overflow from card";

		case 0x30: return "Non negotiable mode (TA2 present)";
		case 0x31: return "Protocol is neither T=0 nor T=1 (negotiate command)";
		case 0x32: return "T=1 is not accepted (negotiate command)";
		case 0x33: return "PPS answer is different from PPS request";
		case 0x34: return "Error on PCK (negotiate command)";
		case 0x35: return "Bad parameter in command";
		case 0x38: return "TB3 absent";
		case 0x39: return "PPS not accepted (no answer from card)";
		case 0x3b: return "Early answer of the card during the activation";

		case 0x40: return "Card Deactivated";

		case 0x55: return "Unknown command";

		case 0x80: return "Card mute (after power on)";
		case 0x81: return "Time out (waiting time exceeded)";
		case 0x83: return "5 parity errors in reception";
		case 0x84: return "5 parity errors in transmission";
		case 0x86: return "Bad FiDi";
		case 0x88: return "ATR duration greater than 19200 etus (E.M.V.)";
		case 0x89: return "CWI not supported (E.M.V.)";
		case 0x8a: return "BWI not supported (E.M.V.)";
		case 0x8b: return "WI (Work waiting time) not supported (E.M.V.)";
		case 0x8c: return "TC3 not accepted (E.M.V.)";
		case 0x8d: return "Parity error during ATR";

		case 0x92: return "Specific mode byte TA2 with b5 byte=1";
		case 0x93: return "TB1 absent during a cold reset (E.M.V.)";
		case 0x94: return "TB1 different from 00 during a cold reset (E.M.V.)";
		case 0x95: return "IFSC<10H or IFSC=FFH";
		case 0x96: return "Wrong TDi";
		case 0x97: return "TB2 is present in the ATR (E.M.V.)";
		case 0x98: return "TC1 is not compatible with CWT";
		case 0x99: return "IFSD not accepted";
		case 0x9b: return "Not T=1 card";

		case 0xa0: return "Procedure byte error";

		case 0xb0: return "Writing attempt in a protected byte (S9 cards)";
		case 0xb1: return "Pin Code error (S9 cards)";
		case 0xb2: return "Writing error (S9 cards)";
		case 0xb3: return "Too much data requested in a reading operation (S9 cards)";
		case 0xb4: return "Error counter protected (S9 cards)";
		case 0xb5: return "Writing attempt without Pin Code verification (S9 cards)";
		case 0xb6: return "Protected bit already set (S9 cards)";
		case 0xb7: return "Verify Pin Code error (S9 cards)";

		case 0xc0: return "Card absent";
		case 0xc1: return "I/O line locked while the TDA8029 attempts to access to an I2C or S10 card";
		case 0xc3: return "Checksum error";
		case 0xc6: return "ATR not supported";
		case 0xcc: return "No acknowledge from the I2C synchronous card";
		case 0xcd: return "Generic error during an exchange with an I2C synchronous card";

		case 0xe1: return "Card clock frequency not accepted (after a set_clock_card command)";
		case 0xe2: return "UART overflow";
		case 0xe3: return "Supply voltage drop-off";
		case 0xe4: return "Temperature alarm";
		case 0xe5: return "Card deactivated";
		case 0xe9: return "Framing error";

		case 0xf0: return "Serial LRC error";
		case 0xf1: return "At least one command frame has been lost";
		case 0xff: return "Serial time out";

		default:   return "Unknown error";
	}
}

static uint8_t sc_lrc(const uint8_t *buf, int len)
{
	uint8_t lrc = 0;
	int i;

	for (i = 0; i < len; i++)
		lrc = lrc ^ buf[i];

	return lrc;
}

static ssize_t sc_read(int fd, uint8_t *buf, int len, int max_wait)
{
	struct timeval tv, *tvp;
	size_t pos = 0;
	fd_set fdset;

	FD_ZERO(&fdset);
	FD_SET(fd, &fdset);

	if (max_wait == -1) {
		tvp = NULL;
	} else {
		tv.tv_sec = max_wait / 1000;
		tv.tv_usec = (max_wait % 1000) * 1000;
		tvp = &tv;
	}

	while (pos < len) {
		int err = select(fd + 1, &fdset, NULL, NULL, tvp);

		if (err < 0) {
			if (err == EINTR)
				continue;
			return err;
		}
		if (err == 0)
			return -ETIMEDOUT;

		if (FD_ISSET(fd, &fdset)) {
			ssize_t count = read(fd, buf + pos, len - pos);
			if (count < 0) {
				if (errno == EINTR)
					continue;
				return -errno;
			}
			pos += count;
		}
	}
	return pos;
}

static int sc_receive(int fd, uint8_t *buf, int *payload_len, int max_wait)
{
	int len = ALPAR_HEADER_LEN;
	ssize_t pos;

	pos = sc_read(fd, buf, len, max_wait);
	if (pos < 0)
		return pos;

	if (buf[0] != ALPAR_ACK && buf[0] != ALPAR_NACK)
		return -EPROTO;

	*payload_len = (buf[1] << 8) | buf[2];
	if (*payload_len > ALPAR_MAX_PAYLOAD)
		return -EMSGSIZE;

	len += *payload_len + 1;
	pos = sc_read(fd, &buf[pos], len - pos, max_wait);
	if (pos < 0)
		return pos;

	if (sc_lrc(buf, len))
		return -EBADMSG;

	if (buf[0] == ALPAR_NACK)
		pr_debug("%s failed with status %d (%s)",  __FUNCTION__,
				buf[4], sc_err_str(buf[4]));

	return 0;
}

static int sc_transmit(int fd, uint8_t cmd, uint8_t *buf, int payload_len)
{
	int ret;

	buf[0] = ALPAR_ACK;
	buf[1] = (payload_len & 0xFF00) >> 8 ;
	buf[2] = payload_len & 0x00FF;
	buf[3] = cmd;

	buf[payload_len + 4] = sc_lrc(buf, payload_len + 4);

	ret = write(fd, buf, payload_len + 5);

	return ret == payload_len + 5 ? 0 : ret < 0 ? ret : -EIO;
}

static int sc_wake_up(int fd)
{
	uint8_t buf[] = { 0xAA };

	return write(fd, buf, sizeof(buf));
}

static void nxp_fire_event(struct smartcard *smartcard, gboolean inserted)
{
	struct event event;
	int err;

	pr_debug("%s: %s",  __FUNCTION__, inserted ? "INSERTED" : "REMOVED");

	memset(&event, 0, sizeof(event));
	event.source = EVENT_SOURCE_SMARTCARD;
	event.smartcard.state = inserted ?
			EVENT_SMARTCARD_STATE_INSERTED :
			EVENT_SMARTCARD_STATE_REMOVED;

	err = event_manager_report(
			remote_control_get_event_manager(smartcard->rc),
			&event);
	if (err < 0 && err != -EBADF)
		g_debug("event_manager_report(): %s",
				strerror(-err));
}

static ssize_t nxp_command(struct smartcard *smartcard, uint8_t cmd,
		uint8_t *buf, int payload_len);

static void nxp_check_card(struct smartcard *smartcard, gboolean inserted)
{
	uint8_t *payload = sc_payload(smartcard->rcv_buffer);
	int retry_bad_fidi;

	smartcard->type = SMARTCARD_TYPE_UNKNOWN;
	smartcard->atr_len = 0;

	if (!inserted) {
		nxp_fire_event(smartcard, inserted);
		return;
	}

	for (retry_bad_fidi = 0; retry_bad_fidi < 3; retry_bad_fidi++) {
		smartcard->rcv_len = nxp_command(smartcard,
				NXP_POWER_UP_ISO,
				smartcard->rcv_buffer, 0);
		if (smartcard->rcv_len != -EREMOTEIO)
			break;
		if (!sc_is_bad_fidi(smartcard->rcv_buffer))
			break;
	};

	if (smartcard->rcv_len < 0) {
		smartcard->rcv_len = nxp_command(smartcard,
				NXP_POWER_UP_I2C,
				smartcard->rcv_buffer, 0);
		if (!smartcard->rcv_len) {
			smartcard->type = SMARTCARD_TYPE_I2C;
			pr_debug("I2C card detected");
		}
	} else {
		uint8_t buf[ALPAR_MAX_BUFFER];
		size_t ret;

		memcpy(smartcard->atr_buffer, payload,
				smartcard->rcv_len);
		smartcard->atr_len = smartcard->rcv_len;

		hexdump("ATR", smartcard->atr_buffer, smartcard->atr_len);

		ret = nxp_command(smartcard, NXP_GET_CARD_PARAM,
				buf, 0);
		if (ret > 2) {
			switch(sc_payload(buf)[2]) {
			case 0:
				smartcard->type = SMARTCARD_TYPE_T0;
				pr_debug("T0 card detected");
				break;
			case 1:
				smartcard->type = SMARTCARD_TYPE_T1;
				pr_debug("T1 card detected");
				break;
			}
		}
	}

	nxp_fire_event(smartcard, inserted);
}

static int nxp_receive(struct smartcard *smartcard, uint8_t *buf,
		int *payload_len, int max_wait)
{
	int ret;

	while (!(ret = sc_receive(smartcard->fd, buf, payload_len, max_wait))) {
		if (sc_is_card_action(buf, *payload_len)) {
			nxp_check_card(smartcard, buf[4]);
			continue;
		}
		if (sc_is_wake_up(buf, *payload_len)) {
			sc_transmit(smartcard->fd, buf[3], buf, *payload_len);
			continue;
		}
		break;
	}

	return ret;
}

static void nxp_flush_response(struct smartcard *smartcard, gboolean new_cmd)
{
	uint8_t buf[ALPAR_MAX_BUFFER];
	gboolean wait = new_cmd;
	int len;

	if (new_cmd)
		sc_wake_up(smartcard->fd);

	while (!nxp_receive(smartcard, buf, &len, wait ? ALPAR_WAIT : 0))
		wait = FALSE;

	if (new_cmd)
		tcflush(smartcard->fd, TCIFLUSH);
}

static ssize_t nxp_command(struct smartcard *smartcard, uint8_t cmd,
		uint8_t *buf, int payload_len)
{
	int ret, len;

	nxp_flush_response(smartcard, TRUE);
	if ((ret = sc_transmit(smartcard->fd, cmd, buf, payload_len)))
		return ret;
	while (!(ret = nxp_receive(smartcard, buf, &len, ALPAR_WAIT)))
		if (buf[3] == cmd)
			break;

	if (buf[0] != ALPAR_ACK)
		return -EREMOTEIO;

	return ret < 0 ? ret : len;
}

static int nxp_check_card_presence(struct smartcard *smartcard)
{
	uint8_t buf[ALPAR_MAX_BUFFER];
	ssize_t ret;

	if ((ret = nxp_command(smartcard, NXP_CHECK_PRES_CARD, buf, 0)) < 0)
		return ret;
	if (ret != 1)
		return -EIO;

	nxp_check_card(smartcard, sc_payload(buf)[0]);

	return 0;
}

static ssize_t nxp_read_i2c(struct smartcard *smartcard, off_t offset,
		void *buffer, size_t size)
{
	uint8_t buf[ALPAR_MAX_BUFFER];
	uint8_t *payload = sc_payload(buf);
	size_t pos = 0;
	uint8_t cmd;
	ssize_t ret;

	if (size > ALPAR_MAX_PAYLOAD)
		size = ALPAR_MAX_PAYLOAD;

	payload[pos++] = 0xA0; // i2c address
	payload[pos++] = (offset & 0xFF00) >> 8;
	payload[pos++] = offset & 0xFF;
	payload[pos++] = (size & 0xFF00) >> 8;
	payload[pos++] = size & 0xFF;
	/* I2C extended does not work on some cards, offset gets ignored */
	cmd = offset > 0xFF ? NXP_READ_I2C_EXTENDED : NXP_READ_I2C;

	if ((ret = nxp_command(smartcard, cmd, buf, pos)) < 0)
		return ret;

	if (ret)
		memcpy(buffer, sc_payload(buf), ret);

	return ret;
}

static ssize_t nxp_read_async(struct smartcard *smartcard, off_t offset,
		void *buffer, size_t size)
{
	uint8_t *payload = sc_payload(smartcard->rcv_buffer);

	if (smartcard->rcv_len <= 0)
		return smartcard->rcv_len;

	if (offset + size > smartcard->rcv_len)
		size = smartcard->rcv_len - offset;

	memcpy(buffer, &payload[offset], size);

	return size;
}


static ssize_t nxp_write_async(struct smartcard *smartcard, off_t offset,
		const void *buffer, size_t size)
{
	uint8_t *payload = sc_payload(smartcard->rcv_buffer);

	if (size > ALPAR_MAX_PAYLOAD)
		size = ALPAR_MAX_PAYLOAD;

	if (smartcard->atr_len > 0 && size == sizeof(NXP_EGK_REQUEST_ICC) &&
			!memcmp(NXP_EGK_REQUEST_ICC, buffer, size)) {
		// Handle IIC request with ATR
		uint8_t *payload = sc_payload(smartcard->rcv_buffer);
		ssize_t count = ALPAR_MAX_PAYLOAD - sizeof(NXP_EGK_SUCCESS);

		if (smartcard->atr_len < count)
			count = smartcard->atr_len;
		memcpy(payload, smartcard->atr_buffer, count);
		payload += count;
		memcpy(payload, NXP_EGK_SUCCESS, sizeof(NXP_EGK_SUCCESS));

		smartcard->rcv_len = sizeof(NXP_EGK_SUCCESS) + count;

		return size;
	}

	if (size)
		memcpy(payload, buffer, size);
	smartcard->rcv_len = nxp_command(smartcard, NXP_CARD_COMMAND,
			smartcard->rcv_buffer, size);

	if (smartcard->rcv_len < 0)
		return smartcard->rcv_len;

	return size;
}

static int nxp_open(struct smartcard *smartcard)
{
	int flags = O_RDWR | O_NONBLOCK;
	struct termios t;

	smartcard->fd = open(smartcard->device, flags, 0);
	if (smartcard->fd == -1)
		return -errno;
	if (tcgetattr(smartcard->fd, &smartcard->sct) < 0) {
		close(smartcard->fd);
		smartcard->fd = -1;

		return -errno;
	}
	t = smartcard->sct;

	cfsetispeed(&t, B38400);
	cfsetospeed(&t, B38400);
	cfmakeraw(&t);
	t.c_cc[VMIN] = 0;
	t.c_cc[VTIME] = 0;

	if (tcsetattr(smartcard->fd, TCSANOW, &t) < 0)
		return -errno;

	return nxp_check_card_presence(smartcard);
}

static gpointer nxp_scan_thread(gpointer data)
{
	struct smartcard *smartcard = data;

	while (!smartcard->scan_done) {
		g_mutex_lock(&smartcard->serial_mutex);
		nxp_flush_response(smartcard, FALSE);
		g_mutex_unlock(&smartcard->serial_mutex);
		g_usleep(1000L * NXP_SCAN_WAIT);
	}

	return NULL;
}

int smartcard_free_nxp(struct smartcard *smartcard)
{
	if (!smartcard)
		return -EINVAL;

	smartcard->scan_done = TRUE;
	g_thread_join(smartcard->scan_thread);

	if (smartcard->fd != -1) {
		tcsetattr(smartcard->fd, TCSANOW, &smartcard->sct);
		close(smartcard->fd);
	}
	g_mutex_clear(&smartcard->serial_mutex);
	g_free(smartcard->device);
	free(smartcard);

	return 0;
}

int smartcard_create_nxp(struct smartcard **smartcardp,
		struct remote_control *rc, GKeyFile *config)
{
	struct smartcard *smartcard;
	int ret;

	if (!smartcardp)
		return -EINVAL;

	if (!g_key_file_has_group(config, "smartcard"))
		pr_debug("Warning: No 'smartcard' section in config");

	smartcard = malloc(sizeof(*smartcard));
	if (!smartcard)
		return -ENOMEM;
	memset(smartcard, 0, sizeof(*smartcard));

	smartcard->fd = -1;
	smartcard->rc = rc;
	g_mutex_init(&smartcard->serial_mutex);

	smartcard->device = g_key_file_get_string(config, "smartcard", "device",
			NULL);
	if (!smartcard->device) {
		pr_debug("Warning: falling back to default device %s",
			SC_NXP_FALLBACK_DEVICE);
		smartcard->device = g_strdup(SC_NXP_FALLBACK_DEVICE);
	}

	ret = nxp_open(smartcard);
	if (ret < 0) {
		pr_debug("Failed to open %s: %d", smartcard->device, ret);
		goto nodevice;
	}

	smartcard->scan_thread = g_thread_new("nxp_scan", nxp_scan_thread,
			smartcard);
	if (!smartcard->scan_thread) {
		pr_debug("Failed to create scan thread");
		goto nodevice;
	}

	pr_debug("%s: Using device %s", __FUNCTION__, smartcard->device);
	*smartcardp = smartcard;

	return 0;

nodevice:
	(void)smartcard_free_nxp(smartcard);

	return -ENODEV;
}

int smartcard_get_type_nxp(struct smartcard *smartcard, unsigned int *typep)
{
	if (!smartcard || !typep)
		return -EINVAL;

	*typep = smartcard->type;

	return 0;
}

ssize_t smartcard_read_nxp(struct smartcard *smartcard, off_t offset,
		void *buffer, size_t size)
{
	ssize_t ret;

	if (!smartcard || !buffer)
		return -EINVAL;

	g_mutex_lock(&smartcard->serial_mutex);
	switch (smartcard->type) {
	case SMARTCARD_TYPE_I2C:
		ret = nxp_read_i2c(smartcard, offset, buffer, size);
		break;
	case SMARTCARD_TYPE_T0:
	case SMARTCARD_TYPE_T1:
		ret = nxp_read_async(smartcard, offset, buffer, size);
		break;
	default:
		ret = -EOPNOTSUPP;
	}
	g_mutex_unlock(&smartcard->serial_mutex);

	return ret;
}

ssize_t smartcard_write_nxp(struct smartcard *smartcard, off_t offset,
		const void *buffer, size_t size)
{
	ssize_t ret;

	if (!smartcard || !buffer)
		return -EINVAL;

	g_mutex_lock(&smartcard->serial_mutex);
	switch (smartcard->type) {
	case SMARTCARD_TYPE_I2C:
		ret = -EOPNOTSUPP;
		break;
	case SMARTCARD_TYPE_T0:
	case SMARTCARD_TYPE_T1:
		ret = nxp_write_async(smartcard, offset, buffer, size);
		break;
	default:
		ret = -EOPNOTSUPP;
	}
	g_mutex_unlock(&smartcard->serial_mutex);

	return ret;
}
