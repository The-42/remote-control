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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <linux/fb.h>
#include <sysfs/libsysfs.h>

#include "remote-control-stub.h"
#include "remote-control.h"

struct backlight {
	struct sysfs_device *device;
	int max_brightness;
};

static int backlight_initialize(struct backlight *backlight)
{
	struct sysfs_class_device *dev;
	struct sysfs_class *cls;
	struct dlist *devlist;
	int ret = -ENODEV;

	cls = sysfs_open_class("backlight");
	if (!cls)
		return ret;

	devlist = sysfs_get_class_devices(cls);
	if (!devlist)
		goto out;

	/* search all backlight devices and take the first one */
	dlist_for_each_data(devlist, dev, struct sysfs_class_device) {
		if (!dev)
			continue;

		backlight->device = sysfs_open_device_tree(dev->path);
		if (backlight->device) {
			ret = 0;
			break;
		}
	}

	if (backlight->device) {
		struct sysfs_attribute *attrib;

		attrib = sysfs_get_device_attr(backlight->device, "max_brightness");
		if (attrib) {
			ret = sysfs_read_attribute(attrib);
			if ((ret < 0) || (attrib->len == 0)) {
				if (attrib->len == 0)
					ret = -ENODATA;
				else
					ret = -errno;

				sysfs_close_attribute(attrib);
				goto out;
			}

			backlight->max_brightness = atoi(attrib->value);
			sysfs_close_attribute(attrib);
		} else {
			backlight->max_brightness = 255;
		}

		g_debug("backlight-sysfs: using device: %s (%s)",
			backlight->device->name, backlight->device->path);
	}

out:
	sysfs_close_class(cls);
	return ret;
}

int backlight_create(struct backlight **backlightp)
{
	struct backlight *blk;
	int err;

	blk = malloc(sizeof(struct backlight));
	if (!blk)
		return -ENOMEM;

	memset(blk, 0, sizeof(*blk));

	err = backlight_initialize(blk);
	if (err < 0) {
		free(blk);
		return err;
	}

	*backlightp = blk;
	return 0;
}

int backlight_free(struct backlight *backlight)
{
	if (!backlight)
		return -EINVAL;

	sysfs_close_device_tree(backlight->device);
	free(backlight);

	return 0;
}

int backlight_enable(struct backlight *backlight, bool enable)
{
	struct sysfs_attribute *attrib;
	char buf[2];
	int err;

	if (!backlight)
		return -EINVAL;

	if (!backlight->device)
		return -ENODEV;

	attrib = sysfs_get_device_attr(backlight->device, "bl_power");
	if (!attrib)
		return -ENOENT;

	if (enable)
		err = snprintf(buf, sizeof(buf), "%u", FB_BLANK_UNBLANK);
	else
		err = snprintf(buf, sizeof(buf), "%u", FB_BLANK_POWERDOWN);

	if (err < 0)
		return -EINVAL;

	buf[err] = '\0';

	err = sysfs_write_attribute(attrib, buf, strlen(buf));
	if (err < 0)
		err = -errno;

	sysfs_close_attribute(attrib);
	return err;
}

int backlight_set(struct backlight *backlight, unsigned int brightness)
{
	struct sysfs_attribute *attrib;
	char value[4];
	int err;

	if (!backlight || brightness > backlight->max_brightness)
		return -EINVAL;

	if (!backlight->device)
		return -ENODEV;

	/* value should not be larger the 4byte since the range is 0...255 */
	snprintf(value, sizeof(value), "%u", brightness);

	attrib = sysfs_get_device_attr(backlight->device, "brightness");
	if (!attrib)
		return -ENOENT;

	err = sysfs_write_attribute(attrib, value, strlen(value));
	if (err < 0)
		err = -errno;

	sysfs_close_attribute(attrib);
	return err;
}

int backlight_get(struct backlight *backlight)
{
	struct sysfs_attribute *attrib;
	int ret;

	if (!backlight)
		return -EINVAL;

	if (!backlight->device)
		return -ENODEV;

	attrib = sysfs_get_device_attr(backlight->device, "actual_brightness");
	if (!attrib)
		return -ENOENT;

	ret = sysfs_read_attribute(attrib);
	if (ret < 0) {
		sysfs_close_attribute(attrib);
		return -errno;
	}

	if (attrib->len > 0)
		ret = atoi(attrib->value);
	else
		ret = -ENODATA;

	sysfs_close_attribute(attrib);
	return ret;
}
