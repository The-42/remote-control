/*
 * Copyright (C) 2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>

#include <libccid.h>

#include "remote-control-stub.h"
#include "remote-control.h"

#define USB_VENDOR_OMNIKEY 0x076b
#define USB_DEVICE_OMNIKEY_CARDMAN_5121 0x5121

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define USE_POLLING_THREAD 1

/* TODO: possibly make this configurable in remote-control.conf */
static const char I2C_DEVICE[] = "/dev/i2c-1";

struct ccid {
	struct libccid_device *device;
	struct remote_control *rc;
	unsigned int refcount;
	struct libccid *ccid;
#ifdef USE_POLLING_THREAD
	gboolean present[2];
	GThread *thread;
	/* FIXME: move locking into libccid? */
	GMutex *mutex;
	gboolean done;
#endif
};

#ifdef USE_POLLING_THREAD
static inline void ccid_lock(struct ccid *ccid)
{
	g_mutex_lock(ccid->mutex);
}

static inline void ccid_unlock(struct ccid *ccid)
{
	g_mutex_unlock(ccid->mutex);
}
#else
static inline void ccid_lock(struct ccid *ccid)
{
}

static inline void ccid_unlock(struct ccid *ccid)
{
}
#endif

#ifdef USE_POLLING_THREAD
static gpointer ccid_thread(gpointer data)
{
	struct event_manager *manager;
	struct ccid *ccid = data;
	ssize_t err;

	manager = remote_control_get_event_manager(ccid->rc);

	while (!ccid->done) {
		struct event event;
		bool changed;

		ccid_lock(ccid);
		err = libccid_device_poll(ccid->device, 0);
		ccid_unlock(ccid);
		if (err < 0) {
		}

		memset(&event, 0, sizeof(event));
		event.source = EVENT_SOURCE_SMARTCARD;
		changed = false;

		if ((err > 0) && !ccid->present[0]) {
			event.smartcard.state = EVENT_SMARTCARD_STATE_INSERTED;
			ccid->present[0] = TRUE;
			changed = true;
		}

		if ((err == 0) && ccid->present[0]) {
			event.smartcard.state = EVENT_SMARTCARD_STATE_REMOVED;
			ccid->present[0] = FALSE;
			changed = true;
		}

		if (changed) {
			err = event_manager_report(manager, &event);
			if (err < 0) {
				if (err != -EBADF) {
					g_debug("event_manager_report(): %s",
							strerror(-err));
				}
			}
		}

		memset(&event, 0, sizeof(event));
		event.source = EVENT_SOURCE_RFID;
		changed = false;

		ccid_lock(ccid);
		err = libccid_device_poll(ccid->device, 1);
		ccid_unlock(ccid);
		if (err < 0) {
		}

		if ((err > 0) && !ccid->present[1]) {
			event.rfid.state = EVENT_RFID_STATE_DETECTED;
			ccid->present[1] = TRUE;
			changed = true;
		}

		if ((err == 0) && ccid->present[1]) {
			event.rfid.state = EVENT_RFID_STATE_LOST;
			ccid->present[1] = FALSE;
			changed = true;
		}

		if (changed) {
			err = event_manager_report(manager, &event);
			if (err < 0) {
				if (err != -EBADF) {
					g_debug("event_manager_report(): %s",
							strerror(-err));
				}
			}
		}

		usleep(250000);
	}

	return NULL;
}
#endif /* USE_POLLING_THREAD */

struct ccid *ccid_ref(struct ccid *ccid)
{
	if (!ccid)
		return NULL;

	ccid->refcount++;
	return ccid;
}

void ccid_unref(struct ccid *ccid)
{
	if (!ccid)
		return;

	ccid->refcount--;

	if (ccid->refcount > 0)
		return;

#ifdef USE_POLLING_THREAD
	ccid->done = TRUE;
	g_thread_join(ccid->thread);
	g_mutex_free(ccid->mutex);
#endif
	libccid_unref(ccid->ccid);
	free(ccid);
}

int ccid_open_device(struct ccid *ccid)
{
	const uint8_t mifare_key[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	int err;

	/*
	 * TODO: Do not hardcode the vendor and device IDs. One possibility
	 *       would be to enumerate the devices using libusb or integrate
	 *       some kind of probing/enumeration into libccid.
	 */
	err = libccid_usb_device_new_with_id(ccid->ccid, &ccid->device,
			USB_VENDOR_OMNIKEY, USB_DEVICE_OMNIKEY_CARDMAN_5121);
	if (!err) {
		g_debug("  using Omnikey Cardman 5121");

		err = libccid_device_mifare_load_key(ccid->device, 1,
				mifare_key, sizeof(mifare_key));
		g_debug("  libccid_device_mifare_load_key(): %d", err);
		if (err < 0) {
			g_warning("  failed to load default MIFARE key: %s",
					strerror(-err));
		}

		return 0;
	}

	err = libccid_i2c_device_new_eeprom(ccid->ccid, &ccid->device,
			I2C_DEVICE);
	if (!err) {
		g_debug("  using I2C EEPROM");
		return 0;
	}

	return -ENODEV;
}

struct ccid *ccid_new(struct remote_control *rc)
{
	static struct ccid *ccid = NULL;
	int err;

	if (ccid)
		return ccid_ref(ccid);

	ccid = calloc(1, sizeof(*ccid));
	if (!ccid)
		return NULL;

	ccid->refcount = 1;
	ccid->rc = rc;

	err = libccid_create(&ccid->ccid);
	if (err < 0) {
		free(ccid);
		return NULL;
	}

	err = ccid_open_device(ccid);
	if (err < 0) {
		libccid_unref(ccid->ccid);
		free(ccid);
		return NULL;
	}

#ifdef USE_POLLING_THREAD
	ccid->done = FALSE;

	ccid->thread = g_thread_create(ccid_thread, ccid, TRUE, NULL);
	if (!ccid->thread) {
		libccid_device_unref(ccid->device);
		libccid_unref(ccid->ccid);
		free(ccid);
		return NULL;
	}

	ccid->mutex = g_mutex_new();
	if (!ccid->mutex) {
		g_thread_join(ccid->thread);
		libccid_device_unref(ccid->device);
		libccid_unref(ccid->ccid);
		free(ccid);
		return NULL;
	}
#endif

	return ccid;
}

struct smartcard {
	struct ccid *ccid;
};

int smartcard_create(struct smartcard **smartcardp, struct rpc_server *server)
{
	struct remote_control *rc = rpc_server_priv(server);
	struct smartcard *smartcard;

	if (!smartcardp)
		return -EINVAL;

	smartcard = calloc(1, sizeof(*smartcard));
	if (!smartcard)
		return -ENOMEM;

	smartcard->ccid = ccid_new(rc);
	if (!smartcard->ccid) {
		free(smartcard);
		return -ENOMEM;
	}

	*smartcardp = smartcard;
	return 0;
}

int smartcard_free(struct smartcard *smartcard)
{
	if (!smartcard)
		return -EINVAL;

	ccid_unref(smartcard->ccid);
	free(smartcard);
	return 0;
}

int smartcard_get_type(struct smartcard *smartcard, unsigned int *typep)
{
	if (!smartcard || !typep)
		return -EINVAL;

	*typep = RPC_MACRO(CARD_TYPE_I2C);
	return 0;
}

ssize_t smartcard_read(struct smartcard *smartcard, off_t offset, void *buffer,
		size_t size)
{
	struct ccid *ccid;
	ssize_t err;

	g_debug("> %s(smartcard=%p, offset=%ld, buffer=%p, size=%zu)", __func__,
			smartcard, offset, buffer, size);

	if (!smartcard) {
		err = -EINVAL;
		goto out;
	}

	ccid = smartcard->ccid;

	err = libccid_device_i2c_read(ccid->device, 0, offset, buffer, size);

out:
	g_debug("< %s() = %zd", __func__, err);
	return err;
}

ssize_t smartcard_write(struct smartcard *smartcard, off_t offset,
		const void *buffer, size_t size)
{
	struct ccid *ccid;
	ssize_t err;

	g_debug("> %s(smartcard=%p, offset=%ld, buffer=%p, size=%zu)", __func__,
			smartcard, offset, buffer, size);

	if (!smartcard) {
		err = -EINVAL;
		goto out;
	}

	ccid = smartcard->ccid;

	err = libccid_device_i2c_write(ccid->device, 0, offset, buffer, size);

out:
	g_debug("< %s() = %zd", __func__, err);
	return err;
}

struct rfid {
	struct ccid *ccid;
};

int rfid_create(struct rfid **rfidp, struct rpc_server *server)
{
	struct remote_control *rc = rpc_server_priv(server);
	struct rfid *rfid;

	if (!rfidp)
		return -EINVAL;

	rfid = calloc(1, sizeof(*rfid));
	if (!rfid)
		return -ENOMEM;

	rfid->ccid = ccid_new(rc);
	if (!rfid->ccid) {
		free(rfid);
		return -ENOMEM;
	}

	*rfidp = rfid;
	return 0;
}

int rfid_free(struct rfid *rfid)
{
	if (!rfid)
		return -EINVAL;

	ccid_unref(rfid->ccid);
	free(rfid);
	return 0;
}

int rfid_get_type(struct rfid *rfid, unsigned int *typep)
{
	return -ENOSYS;
}

static unsigned int mifare_find_block(off_t offset)
{
	unsigned int block = 1;

	while (offset > 16) {
		if (((block + 1) & 0x3) == 0x3)
			block += 2;
		else
			block += 1;

		offset -= 16;
	}

	return block;
}

ssize_t rfid_read(struct rfid *rfid, off_t offset, void *buffer, size_t size)
{
	struct ccid *ccid;
	unsigned int blk;
	unsigned int pos;
	size_t read = 0;
	ssize_t err = 0;

	g_debug("> %s(rfid=%p, offset=%ld, buffer=%p, size=%zu)", __func__,
			rfid, offset, buffer, size);

	if (!rfid) {
		err = -EINVAL;
		goto out;
	}

	ccid = rfid->ccid;

#ifndef USE_POLLING_THREAD
	err = libccid_device_poll(ccid->device, 1);
	if (err < 0)
		goto out;

	if (err == 0) {
		err = -ENODEV;
		goto out;
	}
#endif

	blk = mifare_find_block(offset);
	pos = offset % 16;

	while (read < size) {
		uint8_t block[16];

		ccid_lock(ccid);
		err = libccid_device_mifare_read(ccid->device, 1, blk, block, sizeof(block));
		ccid_unlock(ccid);
		if (err < 0)
			break;

		memcpy(buffer + read, block + pos, min(16 - pos, size - read));
		read += 16 - pos;

		if (((blk + 1) & 0x3) == 0x3)
			blk += 2;
		else
			blk += 1;

		pos = 0;
	}

	err = read;

out:
	g_debug("< %s() = %zd", __func__, err);
	return err;
}

ssize_t rfid_write(struct rfid *rfid, off_t offset, const void *buffer,
		size_t size)
{
	ssize_t err;

	g_debug("> %s(rfid=%p, offset=%ld, buffer=%p, size=%zu)", __func__,
			rfid, offset, buffer, size);

	if (!rfid) {
		err = -EINVAL;
		goto out;
	}

	err = -ENOSYS;

out:
	g_debug("< %s() = %zd", __func__, err);
	return err;
}
