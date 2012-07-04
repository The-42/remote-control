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

#define _GNU_SOURCE
#include <fcntl.h>
#include <glib.h>
#include <unistd.h>
#include <gpio.h>

#include "remote-control-stub.h"
#include "remote-control.h"

#define SYSFS_GPIO_PATH "/sys/class/gpio"
static const char GPIO_GROUP[] = "gpio";

static struct gpio_map {
	enum gpio gpio;
	char *name;
} gpio_list[] = {
	{ GPIO_HANDSET, "handset" },
	{ GPIO_SMARTCARD, "smartcard" },
};

struct gpio_backend {
	GSource source;
	struct gpio_chip *chip;

	guint base;
	struct pollfd gpios[GPIO_NUM];
	guint gpio_map[GPIO_NUM];


	gint *exposed_gpios;
	gsize num_exposed;

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
	guint i;

	for (i = 0; i < G_N_ELEMENTS(gpio_list); i++) {
		struct pollfd *poll = &backend->gpios[gpio_list[i].gpio];
		if (poll->revents & G_IO_ERR)
			return TRUE;
	}

	return FALSE;
}

static int gpio_handle_irq(struct gpio_backend *backend, guint gpio,
				struct event_manager *events)
{
	const char *state = NULL;
	const char *name = NULL;
	struct pollfd *poll;
	struct event event;
	uint8_t value;
	ssize_t err;

	g_return_val_if_fail(gpio < GPIO_NUM, -EINVAL);
	g_return_val_if_fail(backend != NULL, -EINVAL);
	g_return_val_if_fail(events != NULL, -EINVAL);

	poll = &backend->gpios[gpio];
	if (!poll)
		return -EINVAL;

	err = lseek(poll->fd, 0, SEEK_SET);
	if (err < 0)
		return -errno;

	err = read(poll->fd, &value, sizeof(value));
	if (err < 0)
		return -errno;

	value -= '0';

	memset(&event, 0, sizeof(event));

	switch (gpio) {
	case GPIO_HANDSET:
		event.source = EVENT_SOURCE_HOOK;
		name = "HOOK";

		if (value) {
			event.hook.state = EVENT_HOOK_STATE_OFF;
			state = "OFF";
		} else {
			event.hook.state = EVENT_HOOK_STATE_ON;
			state = "ON";
		}
		break;

	case GPIO_SMARTCARD:
		event.source = EVENT_SOURCE_SMARTCARD;
		name = "SMARTCARD";

		if (value) {
			event.smartcard.state = EVENT_SMARTCARD_STATE_REMOVED;
			state = "REMOVED";
		} else {
			event.smartcard.state = EVENT_SMARTCARD_STATE_INSERTED;
			state = "INSERTED";
		}
		break;

	default:
		break;
	}

	if (name) {
		g_debug("gpio-sysfs: %s --> %s", name, state);
		err = event_manager_report(events, &event);
		if (err < 0)
			g_debug("gpio-sysfs: failed to report event: %s",
				g_strerror(-err));
	} else {
		g_debug("gpio-sysfs: GPIO#%u --> %u", gpio, value);
	}

	return 0;
}

static gboolean gpio_source_dispatch(GSource *source, GSourceFunc callback,
		gpointer user_data)
{
	struct gpio_backend *backend = (struct gpio_backend *)source;
	guint i;
	int err;

	for (i = 0; i < G_N_ELEMENTS(gpio_list); i++) {
		struct pollfd *poll = &backend->gpios[gpio_list[i].gpio];

		if (poll->revents & G_IO_ERR) {
			err = gpio_handle_irq(backend, gpio_list[i].gpio, backend->events);
			if (err < 0) {
				g_debug("gpio-sysfs: %s failed: %s",
						"gpio_handle_irq()",
						g_strerror(-err));
			}
		}
	}

	if (callback)
		return callback(user_data);

	return TRUE;
}

static void gpio_source_finalize(GSource *source)
{
	struct gpio_backend *backend = (struct gpio_backend *)source;
	guint i;
	int err;

	for (i = 0; i < backend->num_exposed; i++) {
		err = gpio_free(backend->chip, backend->exposed_gpios[i]);
		if (err < 0) {
			g_debug("gpio-sysfs: %s failed: %s", "gpio_free()",
					g_strerror(-err));
		}
	}

	for (i = 0; i < G_N_ELEMENTS(gpio_list); i++) {
		struct pollfd *poll = &backend->gpios[i];
		gpio_unwatch(poll);
		gpio_free(backend->chip, backend->gpio_map[gpio_list[i].gpio]);
	}

	gpio_chip_close(backend->chip);
	g_free(backend->exposed_gpios);
}

static GSourceFuncs gpio_source_funcs = {
	.prepare = gpio_source_prepare,
	.check = gpio_source_check,
	.dispatch = gpio_source_dispatch,
	.finalize = gpio_source_finalize,
};

int gpio_load_config(struct gpio_backend *backend, GKeyFile *config)
{
	GError *err = NULL;
	guint gpio;
	int i;

	if (!g_key_file_has_group(config, GPIO_GROUP)) {
		g_warning("gpio-sysfs: configuration is missing");
		return -EINVAL;
	}

	backend->base = g_key_file_get_integer(config, GPIO_GROUP, "base",
					    &err);
	if (err != NULL) {
		g_warning("gpio-sysfs: could not load gpio base address (%s)",
			  err->message);
		g_error_free(err);
		return -EINVAL;
	}

	for (i = 0; i < G_N_ELEMENTS(gpio_list); i++) {
		gpio = g_key_file_get_integer(config, GPIO_GROUP,
					gpio_list[i].name, &err);

		if (err != NULL) {
			g_warning("gpio-sysfs: could not load gpio id for %s "
				  "(%s)", gpio_list[i].name, err->message);
			g_error_free(err);
	                err = NULL;
		}
		backend->gpio_map[gpio_list[i].gpio] = gpio;
	}

	backend->exposed_gpios = g_key_file_get_integer_list(config, GPIO_GROUP,
					"expose", &backend->num_exposed, &err);
	if (err != NULL) {
		g_warning("gpio-sysfs: could not load expose list, no gpios will "
			  "be exposed (%s)", err->message);
		g_error_free(err);
		err = NULL;
	}

	return 0;
}

static int initialize_gpio(enum gpio gpio, GSource *source)
{
	struct gpio_backend *backend = (struct gpio_backend*)source;
	int gpio_num;
	int err;

	g_return_val_if_fail(backend != NULL, -EINVAL);
	g_return_val_if_fail(source != NULL, -EINVAL);

	gpio_num = backend->gpio_map[gpio];
	err = gpio_request(backend->chip, gpio_num);
	if (err < 0) {
		g_debug("gpio-sysfs: %s failed: %s", "gpio_request()",
				g_strerror(-err));
		return err;
	}

	err = gpio_watch(backend->chip, gpio_num, &backend->gpios[gpio]);
	if (err < 0) {
		g_debug("gpio-sysfs: %s failed: %s", "gpio_watch()",
				g_strerror(-err));
		goto free;
	}

	err = gpio_enable_irq(backend->chip, gpio_num, IRQ_TYPE_EDGE_BOTH);

	g_debug("gpio-sysfs: GPIO#%u exported, watching...", gpio_num);
	g_source_add_poll(source, (GPollFD*)&backend->gpios[gpio]);

	return 0;

free:
	gpio_free(backend->chip, gpio_num);
	return err;

}

int gpio_backend_create(struct gpio_backend **backendp, struct event_manager *events,
                     GKeyFile *config)
{
	struct gpio_backend *backend;
	GSource *source;
	char *syspath;
	int err;
	guint i;

	g_return_val_if_fail(backendp != NULL, -EINVAL);

	source = g_source_new(&gpio_source_funcs, sizeof(*backend));
	if (!source)
		return -ENOMEM;

	backend = (struct gpio_backend *)source;
	backend->events = events;
	err = gpio_load_config(backend, config);
	if (err < 0)
		goto unref;

	err = asprintf(&syspath, SYSFS_GPIO_PATH "/gpiochip%u", backend->base);
	if (err < 0)
		goto unref;

	err = gpio_chip_open(syspath, &backend->chip);
	if (err < 0) {
		g_warning("gpio-sysfs: %s(%s) failed: %s",
				"gpio_backend_open", syspath, g_strerror(-err));
	}
	free(syspath);

	for (i = 0; i < G_N_ELEMENTS(gpio_list); i++) {
		err = initialize_gpio(gpio_list[i].gpio, source);
		if (err < 0) {
			g_debug("gpio-sysfs: could not initialize gpio %s",
				gpio_list[i].name);
		}
	}

	for (i = 0; i < backend->num_exposed; i++) {
		err = gpio_request(backend->chip, backend->exposed_gpios[i]);
		if ((err < 0) && (err != -EBUSY)) {
			g_debug("gpio-sysfs: %s failed: %s", "gpio_request",
					g_strerror(-err));
		}
	}

	*backendp = backend;
	return 0;

unref:
	g_source_unref(source);
	return -err;
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
	return backend ? backend->num_exposed : -EINVAL;
}

int gpio_backend_direction_input(struct gpio_backend *backend, unsigned int gpio)
{
	int err;

	g_return_val_if_fail(backend != NULL, -EINVAL);
	g_return_val_if_fail(gpio < backend->num_exposed, -EINVAL);

	err = gpio_direction_input(backend->chip, backend->exposed_gpios[gpio]);
	if (err < 0)
		return err;

	return 0;
}

int gpio_backend_direction_output(struct gpio_backend *backend, unsigned int gpio,
		int value)
{
	int err;

	g_return_val_if_fail(backend != NULL, -EINVAL);
	g_return_val_if_fail(gpio < backend->num_exposed, -EINVAL);

	err = gpio_direction_output(backend->chip, backend->exposed_gpios[gpio],
					value);
	if (err < 0)
		return err;

	return 0;
}

int gpio_backend_set_value(struct gpio_backend *backend, unsigned int gpio, int value)
{
	int err;

	g_return_val_if_fail(backend != NULL, -EINVAL);
	g_return_val_if_fail(gpio < backend->num_exposed, -EINVAL);

	err = gpio_set_value(backend->chip, backend->exposed_gpios[gpio], value);
	if (err < 0)
		return err;

	return 0;
}

int gpio_backend_get_value(struct gpio_backend *backend, unsigned int gpio)
{
	int err;

	g_return_val_if_fail(backend != NULL, -EINVAL);
	g_return_val_if_fail(gpio < backend->num_exposed, -EINVAL);

	err = gpio_get_value(backend->chip, backend->exposed_gpios[gpio]);
	if (err < 0)
		return err;

	return !!err;
}
