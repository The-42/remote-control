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

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <linux/fb.h>

#include "remote-control.h"

#define SYSFS_PATH "/sys"

struct backlight {
	int max_brightness;
};

static int backlight_initialize(struct backlight *backlight)
{
	int ret = -ENODEV;
	int err;
	FILE *fd;

	if (!backlight)
		return -EINVAL;

	fd = fopen(SYSFS_PATH "/class/backlight/pwm-backlight/max_brightness",
			   "r");
	if (fd == NULL)
		return -errno;

	err = fscanf (fd, "%d", &backlight->max_brightness);
	if ((err < 0) && (errno != -EBUSY)) {
		ret = -errno;
		goto out;
	}

	ret = 0;

out:
	fclose(fd);
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

	free(backlight);

	return 0;
}

int backlight_enable(struct backlight *backlight, bool enable)
{
	int err;
	int fd;

	if (!backlight)
		return -EINVAL;

	fd = open(SYSFS_PATH "/class/backlight/pwm-backlight/bl_power",
			  O_WRONLY);
	if (fd < 0)
		return -errno;

	err = dprintf(fd, "%u", enable ? FB_BLANK_UNBLANK : FB_BLANK_POWERDOWN);
	if ((err < 0) && (errno != -EBUSY)) {
		err = -errno;
		goto out;
	}

	err = 0;

out:
	close (fd);
	return err;
}

int backlight_is_enabled(struct backlight *backlight)
{
	int enabled;
	int ret;
	int num;
	FILE *fd;

	if (!backlight)
		return -EINVAL;

	fd = fopen(SYSFS_PATH "/class/backlight/pwm-backlight/bl_power", "r");
	if (fd == NULL)
		return -errno;

	num = fscanf(fd, "%d", &enabled);
	if (num == 1)
		ret = enabled == FB_BLANK_UNBLANK ? 1 : 0;
	else
		ret = num < 0 ? -errno : -EIO;

	fclose (fd);
	return ret;
}

int backlight_set(struct backlight *backlight, unsigned int brightness)
{
	int err;
	int fd;

	if (!backlight || brightness > backlight->max_brightness)
		return -EINVAL;

	fd = open(SYSFS_PATH "/class/backlight/pwm-backlight/brightness",
			  O_WRONLY);
	if (fd < 0)
		return -errno;

	err = dprintf(fd, "%u", brightness);
	if ((err < 0) && (errno != -EBUSY)) {
		err = -errno;
		goto out;
	}

	err = 0;

out:
	close (fd);
	return err;
}

int backlight_get(struct backlight *backlight)
{
	int ret;
	int err;
	FILE *fd;

	if (!backlight)
		return -EINVAL;

	fd = fopen(SYSFS_PATH "/class/backlight/pwm-backlight/brightness",
			  "r");
	if (fd == NULL)
		return -errno;

	err = fscanf (fd, "%d", &ret);
	if ((err < 0) && (errno != -EBUSY)) {
		ret = -errno;
		goto out;
	}

out:
	fclose (fd);
	return ret;
}
