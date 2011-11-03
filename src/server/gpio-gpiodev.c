/*
 * Copyright (C) 2010-2011 Avionic Design GmbH
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

struct gpio_source {
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
	struct gpio_source *gpio = (struct gpio_source *)source;

	if (gpio->fd.revents & G_IO_IN)
		return TRUE;

	return FALSE;
}

static gboolean gpio_source_dispatch(GSource *source, GSourceFunc callback, gpointer user_data)
{
	struct gpio_source *gpio = (struct gpio_source *)source;
	enum gpio type = GPIO_UNKNOWN;
	struct gpio_event data;
	struct event event;
	guint i;
	int err;

	err = read(gpio->fd.fd, &data, sizeof(data));
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

	event_manager_report(gpio->events, &event);

	if (callback)
		return callback(user_data);

	return TRUE;
}

static void gpio_source_finalize(GSource *source)
{
	struct gpio_source *gpio = (struct gpio_source *)source;

	if (gpio->fd.fd >= 0)
		close(gpio->fd.fd);
}

static GSourceFuncs gpio_source_funcs = {
	.prepare = gpio_source_prepare,
	.check = gpio_source_check,
	.dispatch = gpio_source_dispatch,
	.finalize = gpio_source_finalize,
};

GSource *gpio_source_new(struct event_manager *events)
{
	struct gpio_source *gpio;
	GSource *source;
	guint i;

	source = g_source_new(&gpio_source_funcs, sizeof(*gpio));
	if (!source)
		return NULL;

	gpio = (struct gpio_source *)source;
	gpio->events = events;

	gpio->fd.fd = open("/dev/gpio-0", O_RDWR);
	if (gpio->fd.fd < 0)
		goto free;

	for (i = 0; i < G_N_ELEMENTS(gpio_list); i++) {
		struct gpio_event enable;
		int err;

		enable.gpio = gpio_list[i].offset;
		enable.value = 1;

		err = ioctl(gpio->fd.fd, GPIO_IOC_ENABLE_IRQ, &enable);
		if (err < 0)
			goto close;
	}

	gpio->fd.events = G_IO_IN | G_IO_HUP | G_IO_ERR;
	g_source_add_poll(source, &gpio->fd);
	return source;

close:
	close(gpio->fd.fd);
free:
	g_source_unref(source);
	return NULL;
}
