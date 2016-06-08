/*
 * Copyright (C) 2010-2012 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <smartcard.h>

#define pr_fmt(fmt) "smartcard-i2c: " fmt

#include "remote-control-stub.h"
#include "remote-control.h"
#include "gsysfsgpio.h"
#include "glogging.h"

struct smartcard {
	struct smartcard_i2c *i2c;
	GSysfsGpio *power_gpio;
};

static void smartcard_enable_power(struct smartcard *smartcard)
{
	if (smartcard && smartcard->power_gpio) {
		GError *error = NULL;

		if (!g_sysfs_gpio_set_value(smartcard->power_gpio, 1, &error)) {
			pr_debug("failed to enable power: %s", error->message);
			g_clear_error(&error);
		}
	}
}

static void smartcard_disable_power(struct smartcard *smartcard)
{
	if (smartcard && smartcard->power_gpio) {
		GError *error = NULL;

		if (!g_sysfs_gpio_set_value(smartcard->power_gpio, 0, &error)) {
			pr_debug("failed to disable power: %s", error->message);
			g_clear_error(&error);
		}
	}
}

int smartcard_create_i2c(struct smartcard **smartcardp,
		struct rpc_server *server, GKeyFile *config)
{
	gchar **parts, *device, *bus;
	struct smartcard *smartcard;
	GError *error = NULL;
	guint slave = 0x50;
	int err;

	if (!smartcardp)
		return -EINVAL;

	if (!g_key_file_has_group(config, "smartcard"))
		return -EIO;

	smartcard = malloc(sizeof(*smartcard));
	if (!smartcard)
		return -ENOMEM;

	memset(smartcard, 0, sizeof(*smartcard));

	device = g_key_file_get_string(config, "smartcard", "device", &error);
	if (!device) {
		free(smartcard);
		return -ENODEV;
	}

	parts = g_strsplit(device, ":", 0);
	if (!parts) {
		free(smartcard);
		g_free(device);
		return -ENOMEM;
	}

	bus = g_strdup(parts[0]);

	if (parts[1]) {
		slave = strtoul(parts[1], NULL, 16);
		if (slave < 0x3 || slave > 0x77) {
			pr_debug("invalid slave address: %s", parts[1]);
			slave = 0x50;
		}
	}

	g_strfreev(parts);
	g_free(device);

	smartcard->power_gpio = g_key_file_get_gpio(config, "smartcard",
						    "power-gpio", &error);
	if (!smartcard->power_gpio) {
		pr_debug("%s", error->message);
		g_clear_error(&error);
	}

	smartcard_enable_power(smartcard);

	err = smartcard_i2c_open(&smartcard->i2c, bus, slave);
	if (err < 0) {
		free(smartcard);
		g_free(bus);
		return err;
	}

	pr_debug("using %s, slave %#x", bus, slave);

	*smartcardp = smartcard;
	g_free(bus);

	return 0;
}

int smartcard_free_i2c(struct smartcard *smartcard)
{
	if (!smartcard)
		return -EINVAL;

	smartcard_i2c_close(smartcard->i2c);
	smartcard_disable_power(smartcard);

	if (smartcard->power_gpio)
		g_object_unref(smartcard->power_gpio);

	free(smartcard);
	return 0;
}

int smartcard_get_type_i2c(struct smartcard *smartcard, unsigned int *typep)
{
	if (!smartcard || !typep)
		return -EINVAL;

	*typep = RPC_MACRO(CARD_TYPE_I2C);
	return 0;
}

ssize_t smartcard_read_i2c(struct smartcard *smartcard, off_t offset,
		       void *buffer, size_t size)
{
	return smartcard_i2c_read(smartcard->i2c, offset, buffer, size);
}

ssize_t smartcard_write_i2c(struct smartcard *smartcard, off_t offset,
			const void *buffer, size_t size)
{
	return smartcard_i2c_write(smartcard->i2c, offset, buffer, size);
}
