/*
 * Copyright (C) 2011-2012 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <fcntl.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#include <X11/Xlib.h>
#include <X11/extensions/dpms.h>
#include <gudev/gudev.h>

#include "remote-control-stub.h"
#include "remote-control.h"
#include "smbus.h"

struct backlight {
	Display *display;
	int fd;

	int (*release)(struct backlight *backlight);
	int (*set)(struct backlight *backlight, unsigned int brightness);
	int (*get)(struct backlight *backlight);
};

static int backlight_dpms_probe(struct backlight *backlight)
{
	int dummy = 0, major = 0, minor = 0;

	if (!backlight)
		return -EINVAL;

	backlight->display = XOpenDisplay(NULL);
	if (!backlight->display)
		return -ENODEV;

	if (!DPMSGetVersion(backlight->display, &major, &minor) ||
	    !DPMSCapable(backlight->display) ||
	    !DPMSQueryExtension(backlight->display, &dummy, &dummy)) {
		XCloseDisplay(backlight->display);
		return -ENOTSUP;
	}

	return 0;
}

static int backlight_dpms_release(struct backlight *backlight)
{
	if (backlight->display)
		XCloseDisplay(backlight->display);

	return 0;
}

static GUdevDevice *sysfs_find_i2c_bus_by_name(const char *name)
{
	const char *const subsystems[] = { "i2c-dev", NULL };
	GUdevDevice *gmbus = NULL;
	GList *list, *node;
	GUdevClient *udev;

	udev = g_udev_client_new(subsystems);
	if (!udev)
		return NULL;

	list = g_udev_client_query_by_subsystem(udev, "i2c-dev");
	for (node = list; node; node = g_list_next(node)) {
		GUdevDevice *device = G_UDEV_DEVICE(node->data);
		const char *sysfs_name;

		sysfs_name = g_udev_device_get_sysfs_attr(device, "name");

		if (sysfs_name && strcmp(sysfs_name, name) == 0) {
			gmbus = g_object_ref(device);
			break;
		}
	}

	g_list_free_full(list, g_object_unref);
	g_object_unref(udev);

	return gmbus;
}

static int backlight_i2c_release(struct backlight *backlight)
{
	if (backlight->fd >= 0)
		close(backlight->fd);

	return 0;
}

static int backlight_i2c_set(struct backlight *backlight, unsigned int brightness)
{
	uint8_t command[2] = { 0xb1, brightness };
	int err;

	err = write(backlight->fd, command, sizeof(command));
	if (err < 0)
		return -errno;

	return 0;
}

static int backlight_i2c_get(struct backlight *backlight)
{
	uint8_t reg = 0xb1;
	uint8_t value = 0;
	int err;

	err = write(backlight->fd, &reg, sizeof(reg));
	if (err < 0)
		return -errno;

	err = read(backlight->fd, &value, sizeof(value));
	if (err < 0)
		return -errno;

	return value;
}

static int i2c_probe_device(int fd, unsigned int slave)
{
	int err;

	err = ioctl(fd, I2C_SLAVE, slave);
	if (err < 0)
		return -errno;

	if ((slave < 0x30 || slave > 0x37) && (slave < 0x50 || slave > 0x5f))
		return i2c_smbus_write_quick(fd, I2C_SMBUS_WRITE);

	return i2c_smbus_read_byte(fd);
}

static int i2c_probe_bus(int fd)
{
	unsigned int i;
	int err;

	g_debug("backlight: probing slaves...");

	for (i = 0x03; i <= 0x77; i++) {
		err = i2c_probe_device(fd, i);
		if (err < 0)
			continue;

		g_debug("backlight:   found: %#02x", i);
	}

	g_debug("backlight: done");

	return 0;
}

static int backlight_i2c_probe(struct backlight *backlight)
{
	unsigned int slave = 0x37;
	GUdevDevice *gmbus;
	const char *device;
	int err;

	gmbus = sysfs_find_i2c_bus_by_name("i915 gmbus panel");
	if (!gmbus)
		return -ENODEV;

	device = g_udev_device_get_device_file(gmbus);

	err = open(device, O_RDWR);
	if (err < 0) {
		err = -errno;
		goto unref;
	}

	backlight->fd = err;

	/*
	 * FIXME: It looks like the current implementation requires a probe
	 *        of the complete I2C bus before the backlight controller
	 *        (0x37) can be accessed.
	 */
	err = i2c_probe_bus(backlight->fd);
	if (err < 0)
		goto close;

	err = i2c_probe_device(backlight->fd, slave);
	if (err < 0)
		goto close;

	backlight->release = backlight_i2c_release;
	backlight->set = backlight_i2c_set;
	backlight->get = backlight_i2c_get;

	g_debug("backlight: using I2C bus %s, slave %#x", device, slave);
	g_object_unref(gmbus);

	return 0;

close:
	close(backlight->fd);
unref:
	g_object_unref(gmbus);
	return err;
}

static int backlight_probe(struct backlight *backlight)
{
	int err;

	err = backlight_dpms_probe(backlight);
	if (err < 0)
		return err;

	err = backlight_i2c_probe(backlight);
	if (err < 0) {
		backlight_remove_dpms(backlight);
		return err;
	}

	return 0;
}

int backlight_create(struct backlight **backlightp)
{
	struct backlight *backlight;
	int err;

	if (!backlightp)
		return -EINVAL;

	backlight = g_new0(struct backlight, 1);
	if (!backlight)
		return -ENOMEM;

	err = backlight_probe(backlight);
	if (err < 0) {
		g_free(backlight);
		return -err;
	}

	*backlightp = backlight;

	return 0;
}

int backlight_free(struct backlight *backlight)
{
	int err;

	if (!backlight)
		return -EINVAL;

	err = backlight->release(backlight);
	if (err < 0)
		return err;

	err = backlight_dpms_release(backlight);
	if (err < 0)
		return err;

	g_free(backlight);

	return 0;
}

int backlight_enable(struct backlight *backlight, bool enable)
{
	CARD8 mode = enable ? DPMSModeOn : DPMSModeOff;

	if (!backlight)
		return -EINVAL;

	DPMSForceLevel(backlight->display, mode);
	XFlush(backlight->display);

	return 0;
}

int backlight_set(struct backlight *backlight, unsigned int brightness)
{
	if (!backlight || brightness > 255)
		return -EINVAL;

	return backlight->set(backlight, brightness);
}

int backlight_get(struct backlight *backlight)
{
	if (!backlight)
		return -EINVAL;

	return backlight->get(backlight);
}
