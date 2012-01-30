/*
 * Copyright (C) 2012 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "gpiolib.h"

/**
 * gpio_request -
 * @gpio:
 */
int gpio_request(unsigned int gpio)
{
	int err;
	int fd;

	fd = open(SYSFS_GPIO_PATH "/export", O_WRONLY);
	if (fd < 0)
		return -errno;

	err = dprintf(fd, "%u", gpio);
	if (err < 0) {
		err = -errno;
		goto out;
	}

	err = 0;

out:
	close(fd);
	return err;
}

/**
 * gpio_free -
 * @gpio:
 */
int gpio_free(unsigned int gpio)
{
	int err;
	int fd;

	fd = open(SYSFS_GPIO_PATH "/unexport", O_WRONLY);
	if (fd < 0)
		return -errno;

	err = dprintf(fd, "%u", gpio);
	if (err < 0) {
		err = -errno;
		goto out;
	}

	err = 0;

out:
	close(fd);
	return err;
}

/**
 * gpio_direction_input -
 * @gpio:
 */
int gpio_direction_input(unsigned int gpio)
{
	char *buffer;
	int err;
	int fd;

	err = asprintf(&buffer, SYSFS_GPIO_PATH "/gpio%u/direction", gpio);
	if (err < 0)
		return -errno;

	fd = open(buffer, O_WRONLY);
	if (fd < 0) {
		err = -errno;
		free(buffer);
		return err;
	}

	free(buffer);

	err = dprintf(fd, "in");
	if (err < 0) {
		err = -errno;
		goto out;
	}

out:
	close(fd);
	return err;
}

/**
 * gpio_direction_output -
 * @gpio:
 * @value:
 */
int gpio_direction_output(unsigned int gpio, int value)
{
	char *buffer;
	int err;
	int fd;

	err = asprintf(&buffer, SYSFS_GPIO_PATH "/gpio%u/direction", gpio);
	if (err < 0)
		return -errno;

	fd = open(buffer, O_WRONLY);
	if (fd < 0) {
		err = -errno;
		free(buffer);
		return err;
	}

	free(buffer);

	err = dprintf(fd, "%s", value ? "high" : "low");
	if (err < 0) {
		err = -errno;
		goto out;
	}

out:
	close(fd);
	return err;
}

/**
 * gpio_get_value -
 * @gpio:
 */
int gpio_get_value(unsigned int gpio)
{
	char *buffer;
	char value;
	int err;
	int fd;

	err = asprintf(&buffer, SYSFS_GPIO_PATH "/gpio%u/value", gpio);
	if (err < 0)
		return -errno;

	fd = open(buffer, O_RDONLY);
	if (fd < 0) {
		err = -errno;
		free(buffer);
		return err;
	}

	free(buffer);

	err = read(fd, &value, sizeof(value));
	if (err <= 0) {
		if (err == 0)
			err = -ENODATA;
		else
			err = -errno;
	} else {
		err = value - '0';
	}

	close(fd);
	return err;
}

/**
 * gpio_set_value -
 * @gpio:
 * @value:
 */
int gpio_set_value(unsigned int gpio, int value)
{
	char *buffer;
	int err;
	int fd;

	err = asprintf(&buffer, SYSFS_GPIO_PATH "/gpio%u/value", gpio);
	if (err < 0)
		return -errno;

	fd = open(buffer, O_WRONLY);
	if (fd < 0) {
		err = -errno;
		free(buffer);
		return err;
	}

	free(buffer);

	err = dprintf(fd, "%d", value);
	if (err < 0)
		err = -errno;
	else
		err = 0;

	close(fd);
	return err;
}
