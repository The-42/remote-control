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

#include "gsysfsbacklight.h"

#include "remote-control-stub.h"
#include "remote-control.h"
#include "gdevicetree.h"
#include "smbus.h"

struct backlight {
	GSysfsBacklight *sysfs;
	Display *display;
	int restore;
	int fd;

	int (*release)(struct backlight *backlight);
	int (*set)(struct backlight *backlight, unsigned int brightness);
	int (*get)(struct backlight *backlight);
	int (*enable)(struct backlight *backlight, bool enable);
	int (*is_enabled)(struct backlight *backlight);
};

static int backlight_dpms_release(struct backlight *backlight)
{
	if (backlight->display)
		XCloseDisplay(backlight->display);

	return 0;
}

static int backlight_dpms_enable(struct backlight *backlight, bool enable)
{
	CARD16 mode = enable ? DPMSModeOn : DPMSModeOff;
	Status ret;

	ret = DPMSForceLevel(backlight->display, mode);
	XFlush(backlight->display);

	return ret ? 0 : -EIO;
}

static int backlight_dpms_set(struct backlight *backlight,
			      unsigned int brightness)
{
	return backlight_dpms_enable(backlight, (brightness > BACKLIGHT_MIN));
}

static int backlight_dpms_get(struct backlight *backlight)
{
	CARD16 level = DPMSModeOff;
	BOOL state = False;

	if (!DPMSInfo(backlight->display, &level, &state))
		return -ENOTSUP;

	switch (level) {
	case DPMSModeOn:
		return BACKLIGHT_MAX;

	case DPMSModeStandby:
	case DPMSModeSuspend:
	case DPMSModeOff:
		return BACKLIGHT_MIN;

	default:
		return -EIO;
	}
}

static int backlight_dpms_is_enabled(struct backlight *backlight)
{
	int state = backlight_dpms_get(backlight);

	if (state > 0)
		return 1;

	return state;
}

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

	backlight->release = backlight_dpms_release;
	backlight->set = backlight_dpms_set;
	backlight->get = backlight_dpms_get;
	backlight->enable = backlight_dpms_enable;
	backlight->is_enabled = backlight_dpms_is_enabled;

	g_debug("backlight-dpms: using DPMS v%d.%d", major, minor);

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

static int backlight_i2c_enable(struct backlight *backlight, bool enable)
{
	uint8_t command[2] = { 0xb1, 0x00 };
	int err;

	if (enable)
		command[1] = 0xff;

	err = write(backlight->fd, command, sizeof(command));
	if (err < 0)
		return -errno;

	return 0;
}

static int backlight_i2c_is_enabled(struct backlight *backlight)
{
	int status = backlight_i2c_get(backlight);

	if (status < 0)
		return status;

	if ((status & 0xFF) > 0)
		return 1;

	return 0;
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

	g_debug("backlight-i2c: probing slaves...");

	for (i = 0x03; i <= 0x77; i++) {
		err = i2c_probe_device(fd, i);
		if (err < 0)
			continue;

		g_debug("backlight-i2c: found: %#02x", i);
	}

	g_debug("backlight-i2c: done");

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
	backlight->enable = backlight_i2c_enable;
	backlight->is_enabled = backlight_i2c_is_enabled;

	g_debug("backlight-i2c: using I2C bus %s, slave %#x", device, slave);
	g_object_unref(gmbus);

	return 0;

close:
	close(backlight->fd);
unref:
	g_object_unref(gmbus);
	return err;
}

static int backlight_sysfs_release(struct backlight *backlight)
{
	if (backlight->sysfs)
		g_object_unref(backlight->sysfs);

	return 0;
}

static int backlight_sysfs_set(struct backlight *backlight,
			       unsigned int brightness)
{
	GSysfsBacklight *bl = backlight->sysfs;
	GError *error = NULL;
	guint value, max;

	max = g_sysfs_backlight_get_max_brightness(bl);
	g_return_val_if_fail(max != 0, -EIO);

	/* Because max can be significantly larger than the actual backlight
	 * value. So we round up here, to avoid a constant decrease of value.
	 */
	value = (brightness * max + 254) / 255;

	if (!g_sysfs_backlight_set_brightness(bl, value, &error)) {
		g_debug("backlight-sysfs: failed to set brightness: %s",
			error->message);
		g_clear_error(&error);
		return -EIO;
	}

	return 0;
}

static int backlight_sysfs_get(struct backlight *backlight)
{
	GSysfsBacklight *bl = backlight->sysfs;
	GError *error = NULL;
	guint value, max;

	max = g_sysfs_backlight_get_max_brightness(bl);
	g_return_val_if_fail(max != 0, -EIO);

	value = g_sysfs_backlight_get_brightness(bl, &error);
	if (error) {
		g_debug("backlight-sysfs: failed to get brightness: %s",
			error->message);
		g_clear_error(&error);
		return -EIO;
	}

	return (value * 255) / max;
}

static int backlight_sysfs_enable(struct backlight *backlight, bool enable)
{
	GSysfsBacklight *bl = backlight->sysfs;
	GError *error = NULL;

	if (enable)
		g_sysfs_backlight_enable(bl, &error);
	else
		g_sysfs_backlight_disable(bl, &error);

	if (error) {
		g_warning("backlight-sysfs: failed to %sable backlight: %s",
			  enable ? "en" : "dis", error->message);
		g_clear_error(&error);
		return -EIO;
	}

	return 0;
}

static int backlight_sysfs_is_enabled(struct backlight *backlight)
{
	GSysfsBacklight *bl = backlight->sysfs;
	GError *error = NULL;
	gboolean state;

	state = g_sysfs_backlight_is_enabled(bl, &error);

	if (error) {
		g_warning("backlight-sysfs: failed to get backlight status: %s",
			  error->message);
		g_clear_error(&error);
		return -EIO;
	}

	return state ? 1 : 0;
}

static int backlight_sysfs_probe(struct backlight *backlight)
{
	GError *error = NULL;
	guint max_brightness;
	GUdevDevice *device;
	const gchar *path;
	GDeviceTree *dt;

	dt = g_device_tree_load(NULL);
	if (dt) {
		if (g_device_tree_is_compatible(dt, "ad,medatom")) {
			g_device_tree_free(dt);
			return -ENOTSUP;
		}

		g_device_tree_free(dt);
	}

	backlight->sysfs = g_sysfs_backlight_new("intel_backlight", &error);
	if (!backlight->sysfs) {
		g_debug("backlight-sysfs: failed to create sysfs backlight:"
			" %s", error->message);
		g_clear_error(&error);
		return -ENODEV;
	}

	backlight->release = backlight_sysfs_release;
	backlight->set = backlight_sysfs_set;
	backlight->get = backlight_sysfs_get;
	backlight->enable = backlight_sysfs_enable;
	backlight->is_enabled = backlight_sysfs_is_enabled;

	backlight->restore = backlight_sysfs_get(backlight);
	if (backlight->restore < 0)
		backlight->restore = 0xff;

	g_object_get(backlight->sysfs, "device", &device, "max-brightness",
		     &max_brightness, NULL);
	path = g_udev_device_get_sysfs_path(device);

	g_debug("backlight: using sysfs %s, maximum brightness %u", path,
		max_brightness);

	g_object_unref(device);

	return 0;
}

static int backlight_probe(struct backlight *backlight)
{
	int err;

	err = backlight_sysfs_probe(backlight);
	if (!err)
		return 0;

	err = backlight_i2c_probe(backlight);
	if (!err)
		return 0;

	err = backlight_dpms_probe(backlight);
	if (!err)
		return 0;

	return err;
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

	g_free(backlight);

	return 0;
}

int backlight_enable(struct backlight *backlight, bool enable)
{
	if (!backlight)
		return -EINVAL;

	return backlight->enable(backlight, enable);
}

int backlight_is_enabled(struct backlight *backlight)
{
	if (!backlight)
		return -EINVAL;

	return backlight->is_enabled(backlight);
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
