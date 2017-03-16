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

#include "remote-control.h"

static struct gpio_map {
	unsigned int offset;
	enum gpio gpio;
} gpio_list[] = {
	{ 0, GPIO_HANDSET },
	{ 2, GPIO_SMARTCARD },
};

static const unsigned int gpios[] = { 34, 35, 36 };

struct gpio_backend {
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
	struct gpio_backend *backend = (struct gpio_backend *)source;

	if (backend->fd.revents & G_IO_IN)
		return TRUE;

	return FALSE;
}

static gboolean gpio_source_dispatch(GSource *source, GSourceFunc callback, gpointer user_data)
{
	struct gpio_backend *backend = (struct gpio_backend *)source;
	enum gpio type = GPIO_UNKNOWN;
	struct gpio_event data;
	struct event event;
	guint i;
	int err;

	err = read(backend->fd.fd, &data, sizeof(data));
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
		if (data.value)
			event.hook.state = EVENT_HOOK_STATE_OFF;
		else
			event.hook.state = EVENT_HOOK_STATE_ON;
		break;

	case GPIO_SMARTCARD:
		event.source = EVENT_SOURCE_SMARTCARD;
		if (data.value)
			event.smartcard.state = EVENT_SMARTCARD_STATE_REMOVED;
		else
			event.smartcard.state = EVENT_SMARTCARD_STATE_INSERTED;
		break;

	default:
		g_debug("gpiodev: unknown GPIO %u transitioned to %u",
				data.gpio, data.value);
		break;
	}

	err = event_manager_report(backend->events, &event);
	if (err < 0)
		g_debug("gpiodev: failed to report event: %s",
			g_strerror(-err));

	if (callback)
		return callback(user_data);

	return TRUE;
}

static void gpio_source_finalize(GSource *source)
{
	struct gpio_backend *backend = (struct gpio_backend *)source;

	if (backend->fd.fd >= 0)
		close(backend->fd.fd);
}

static GSourceFuncs gpio_source_funcs = {
	.prepare = gpio_source_prepare,
	.check = gpio_source_check,
	.dispatch = gpio_source_dispatch,
	.finalize = gpio_source_finalize,
};

int gpio_backend_create(struct gpio_backend **backendp, struct event_manager *events,
		     GKeyFile *config)
{
	struct gpio_backend *backend;
	GSource *source;
	int err;
	guint i;

	g_return_val_if_fail(backendp != NULL, -EINVAL);

	source = g_source_new(&gpio_source_funcs, sizeof(*backend));
	if (!source)
		return -ENOMEM;

	backend = (struct gpio_backend *)source;
	backend->events = events;

	backend->fd.fd = open("/dev/gpio-0", O_RDWR);
	if (backend->fd.fd < 0) {
		err = -errno;
		goto free;
	}

	for (i = 0; i < G_N_ELEMENTS(gpio_list); i++) {
		struct gpio_event enable;

		enable.gpio = gpio_list[i].offset;
		enable.value = 1;

		err = ioctl(backend->fd.fd, GPIO_IOC_ENABLE_IRQ, &enable);
		if (err < 0) {
			err = -errno;
			goto close;
		}
	}

	backend->fd.events = G_IO_IN | G_IO_HUP | G_IO_ERR;
	g_source_add_poll(source, &backend->fd);

	*backendp = backend;
	return 0;

close:
	close(backend->fd.fd);
free:
	g_source_unref(source);
	return err;
}

int gpio_backend_free(struct gpio_backend *backend)
{
	return 0;
}

GSource *gpio_backend_get_source(struct gpio_backend *backend)
{
	return backend ? &backend->source : NULL;
}

int gpio_backend_get_num_gpios(struct gpio_backend *backend)
{
	return backend ? G_N_ELEMENTS(gpios) : -EINVAL;
}

int gpio_backend_direction_input(struct gpio_backend *backend, unsigned int gpio)
{
	int err;

	g_return_val_if_fail(gpio < G_N_ELEMENTS(gpios), -EINVAL);

	err = ioctl(backend->fd.fd, GPIO_IOC_SET_INPUT, gpios[gpio]);
	if (err < 0)
		return -errno;

	return 0;
}

int gpio_backend_direction_output(struct gpio_backend *backend, unsigned int gpio,
		int value)
{
	struct gpio_event pin;
	int err;

	g_return_val_if_fail(gpio < G_N_ELEMENTS(gpios), -EINVAL);

	pin.gpio = gpios[gpio];
	pin.value = !!value;

	err = ioctl(backend->fd.fd, GPIO_IOC_SET_OUTPUT, &pin);
	if (err < 0)
		return -errno;

	return 0;
}

int gpio_backend_set_value(struct gpio_backend *backend, unsigned int gpio, int value)
{
	struct gpio_event pin;
	int err;

	g_return_val_if_fail(gpio < G_N_ELEMENTS(gpios), -EINVAL);

	pin.gpio = gpios[gpio];
	pin.value = !!value;

	err = ioctl(backend->fd.fd, GPIO_IOC_SET_VALUE, &pin);
	if (err < 0)
		return -errno;

	return 0;
}

int gpio_backend_get_value(struct gpio_backend *backend, unsigned int gpio)
{
	int err;

	g_return_val_if_fail(gpio < G_N_ELEMENTS(gpios), -EINVAL);

	err = ioctl(backend->fd.fd, GPIO_IOC_GET_VALUE, gpios[gpio]);
	if (err < 0)
		return -errno;

	return !!err;
}
