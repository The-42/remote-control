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

#include <glib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/gpiodev.h>

#include "remote-control-stub.h"
#include "remote-control.h"

static struct gpio_map {
	unsigned int offset;
	enum gpio gpio;
} gpio_list[] = {
	{ 0, GPIO_HANDSET },
	{ 2, GPIO_SMARTCARD },
};

static const unsigned int gpios[] = { 34, 35, 36 };

struct gpio_chip {
	GSource source;
	GPollFD fd;

	struct event_manager *events;
};

static gboolean gpio_source_prepare(GSource *source, gint *timeout)
{
	if (timeout)
		*timeout = -1;

	return FALSE;
}

static gboolean gpio_source_check(GSource *source)
{
	struct gpio_chip *chip = (struct gpio_chip *)source;

	if (chip->fd.revents & G_IO_IN)
		return TRUE;

	return FALSE;
}

static gboolean gpio_source_dispatch(GSource *source, GSourceFunc callback, gpointer user_data)
{
	struct gpio_chip *chip = (struct gpio_chip *)source;
	enum gpio type = GPIO_UNKNOWN;
	struct gpio_event data;
	struct event event;
	guint i;
	int err;

	err = read(chip->fd.fd, &data, sizeof(data));
	if (err < 0) {
		g_warning("gpiodev: read(): %s", strerror(errno));
		return TRUE;
	}

	for (i = 0; i < G_N_ELEMENTS(gpio_list); i++) {
		if (data.gpio == gpio_list[i].offset) {
			type = gpio_list[i].gpio;
			break;
		}
	}

	switch (type) {
	case GPIO_HANDSET:
		event.source = EVENT_SOURCE_HOOK;
		if (data.value) {
			g_debug("gpiodev: HOOK transitioned to OFF state");
			event.hook.state = EVENT_HOOK_STATE_OFF;
		} else {
			g_debug("gpiodev: HOOK transitioned to ON state");
			event.hook.state = EVENT_HOOK_STATE_ON;
		}
		break;

	case GPIO_SMARTCARD:
		event.source = EVENT_SOURCE_SMARTCARD;
		if (data.value) {
			g_debug("gpiodev: SMARTCARD transitioned to REMOVED state");
			event.smartcard.state = EVENT_SMARTCARD_STATE_REMOVED;
		} else {
			g_debug("gpiodev: SMARTCARD transitioned to INSERTED state");
			event.smartcard.state = EVENT_SMARTCARD_STATE_INSERTED;
		}
		break;

	default:
		g_debug("gpiodev: unknown GPIO %u transitioned to %u",
				data.gpio, data.value);
		break;
	}

	err = event_manager_report(chip->events, &event);
	if (err < 0)
		g_debug("gpiodev: failed to report event: %s",
			g_strerror(-err));

	if (callback)
		return callback(user_data);

	return TRUE;
}

static void gpio_source_finalize(GSource *source)
{
	struct gpio_chip *chip = (struct gpio_chip *)source;

	if (chip->fd.fd >= 0)
		close(chip->fd.fd);
}

static GSourceFuncs gpio_source_funcs = {
	.prepare = gpio_source_prepare,
	.check = gpio_source_check,
	.dispatch = gpio_source_dispatch,
	.finalize = gpio_source_finalize,
};

int gpio_chip_create(struct gpio_chip **chipp, struct event_manager *events,
		     GKeyFile *config)
{
	struct gpio_chip *chip;
	GSource *source;
	int err;
	guint i;

	g_return_val_if_fail(chipp != NULL, -EINVAL);

	source = g_source_new(&gpio_source_funcs, sizeof(*chip));
	if (!source)
		return -ENOMEM;

	chip = (struct gpio_chip *)source;
	chip->events = events;

	chip->fd.fd = open("/dev/gpio-0", O_RDWR);
	if (chip->fd.fd < 0) {
		err = -errno;
		goto free;
	}

	for (i = 0; i < G_N_ELEMENTS(gpio_list); i++) {
		struct gpio_event enable;

		enable.gpio = gpio_list[i].offset;
		enable.value = 1;

		err = ioctl(chip->fd.fd, GPIO_IOC_ENABLE_IRQ, &enable);
		if (err < 0) {
			err = -errno;
			goto close;
		}
	}

	chip->fd.events = G_IO_IN | G_IO_HUP | G_IO_ERR;
	g_source_add_poll(source, &chip->fd);

	*chipp = chip;
	return 0;

close:
	close(chip->fd.fd);
free:
	g_source_unref(source);
	return err;
}

int gpio_chip_free(struct gpio_chip *chip)
{
	return 0;
}

GSource *gpio_chip_get_source(struct gpio_chip *chip)
{
	return chip ? &chip->source : NULL;
}

int gpio_chip_get_num_gpios(struct gpio_chip *chip)
{
	return chip ? G_N_ELEMENTS(gpios) : -EINVAL;
}

int gpio_chip_direction_input(struct gpio_chip *chip, unsigned int gpio)
{
	int err;

	g_return_val_if_fail(gpio < G_N_ELEMENTS(gpios), -EINVAL);

	err = ioctl(chip->fd.fd, GPIO_IOC_SET_INPUT, gpios[gpio]);
	if (err < 0)
		return -errno;

	return 0;
}

int gpio_chip_direction_output(struct gpio_chip *chip, unsigned int gpio,
		int value)
{
	struct gpio_event pin;
	int err;

	g_return_val_if_fail(gpio < G_N_ELEMENTS(gpios), -EINVAL);

	pin.gpio = gpios[gpio];
	pin.value = !!value;

	err = ioctl(chip->fd.fd, GPIO_IOC_SET_OUTPUT, &pin);
	if (err < 0)
		return -errno;

	return 0;
}

int gpio_chip_set_value(struct gpio_chip *chip, unsigned int gpio, int value)
{
	struct gpio_event pin;
	int err;

	g_return_val_if_fail(gpio < G_N_ELEMENTS(gpios), -EINVAL);

	pin.gpio = gpios[gpio];
	pin.value = !!value;

	err = ioctl(chip->fd.fd, GPIO_IOC_SET_VALUE, &pin);
	if (err < 0)
		return -errno;

	return 0;
}

int gpio_chip_get_value(struct gpio_chip *chip, unsigned int gpio)
{
	int err;

	g_return_val_if_fail(gpio < G_N_ELEMENTS(gpios), -EINVAL);

	err = ioctl(chip->fd.fd, GPIO_IOC_GET_VALUE, gpios[gpio]);
	if (err < 0)
		return -errno;

	return !!err;
}
