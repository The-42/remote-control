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
#include <unistd.h>

#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#include "remote-control-stub.h"
#include "remote-control.h"
#include "smbus.h"

struct backlight {
	int fd;
};

int backlight_create(struct backlight **backlightp)
{
	struct backlight *backlight;
	int err;
	int i;

	if (!backlightp)
		return -EINVAL;

	backlight = calloc(1, sizeof(*backlight));
	if (!backlight)
		return -ENOMEM;

	/*
	 * TODO: Autodetect this because it may not always be assigned the
	 *       same bus number.
	 */
	err = open("/dev/i2c-6", O_RDWR);
	if (err < 0) {
		err = -errno;
		goto free;
	}

	backlight->fd = err;

	/*
	 * FIXME: It looks like the current implementation requires a probe
	 *        of the complete I2C bus before the backlight controller
	 *        (0x37)can be accessed.
	 */
	g_debug("%s(): probing slaves...", __func__);

	for (i = 0x03; i <= 0x77; i++) {
		err = ioctl(backlight->fd, I2C_SLAVE, i);
		if (err < 0)
			continue;

		if (((i >= 0x30) && (i <= 0x37)) || ((i >= 0x50) && (i <= 0x5f)))
			err = i2c_smbus_read_byte(backlight->fd);
		else
			err = i2c_smbus_write_quick(backlight->fd, I2C_SMBUS_WRITE);

		if (err < 0)
			continue;

		g_debug("%s():  found: %#02x", __func__, i);
	}

	g_debug("%s(): done", __func__);

	err = ioctl(backlight->fd, I2C_SLAVE, 0x37);
	if (err < 0) {
		err = -errno;
		goto close;
	}

	*backlightp = backlight;
	return 0;

close:
	close(backlight->fd);
free:
	free(backlight);
	return err;
}

int backlight_free(struct backlight *backlight)
{
	if (!backlight)
		return -EINVAL;

	close(backlight->fd);
	free(backlight);

	return 0;
}

int backlight_enable(struct backlight *backlight, bool enable)
{
	uint8_t command[2] = { 0xb1, 0x00 };
	int err;

	if (!backlight)
		return -EINVAL;

	if (enable)
		command[1] = 0xff;

	err = write(backlight->fd, command, sizeof(command));
	if (err < 0)
		return -errno;

	return 0;
}

int backlight_set(struct backlight *backlight, unsigned int brightness)
{
	uint8_t command[2] = { 0xb1, brightness };
	int err;

	if (!backlight)
		return -EINVAL;

	err = write(backlight->fd, command, sizeof(command));
	if (err < 0)
		return -errno;

	return 0;
}

int backlight_get(struct backlight *backlight)
{
	uint8_t reg = 0xb1;
	uint8_t value = 0;
	int err;

	if (!backlight)
		return -EINVAL;

	err = write(backlight->fd, &reg, sizeof(reg));
	if (err < 0)
		return -errno;

	err = read(backlight->fd, &value, sizeof(value));
	if (err < 0)
		return -errno;

	return value;
}
