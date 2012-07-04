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

#include "remote-control-stub.h"
#include "remote-control.h"
#include "gpiolib.h"

#define IRQ_TYPE_EDGE_NONE    0
#define IRQ_TYPE_EDGE_RISING  (1 << 0)
#define IRQ_TYPE_EDGE_FALLING (1 << 1)
#define IRQ_TYPE_EDGE_BOTH    (IRQ_TYPE_EDGE_RISING | IRQ_TYPE_EDGE_FALLING)

static const char GPIO_GROUP[] = "gpio";

static struct gpio_map {
	enum gpio gpio;
	char *name;
} gpio_list[] = {
	{ GPIO_HANDSET, "handset" },
	{ GPIO_SMARTCARD, "smartcard" },
};

struct gpio_desc {
	GPollFD poll;
	enum gpio type;
	guint num;
};

static int gpio_export(struct gpio_desc **gpiop, guint num)
{
	struct gpio_desc *gpio;
	int err;

	gpio = g_new0(struct gpio_desc, 1);
	if (!gpio)
		return -ENOMEM;

	gpio->poll.fd = -1;
	gpio->num = num;

	err = gpio_request(num);
	if ((err < 0) && (err != -EBUSY)) {
		g_free(gpio);
		return err;
	}

	*gpiop = gpio;
	return 0;
}

static int gpio_unexport(struct gpio_desc *gpio)
{
	int err;

	if (!gpio)
		return -EINVAL;

	if (gpio->poll.fd >= 0)
		close(gpio->poll.fd);

	err = gpio_free(gpio->num);
	if (err < 0) {
		g_debug("gpio-sysfs: %s failed: %s", "gpio_sysfs_unexport()",
				g_strerror(-err));
	}

	g_free(gpio);
	return 0;
}

static int gpio_watch(struct gpio_desc *gpio, unsigned int edge)
{
	const char *type = NULL;
	char *buffer = NULL;
	int err;
	int fd;

	if (!gpio)
		return -EINVAL;

	if (gpio->poll.fd >= 0)
		return -EBUSY;

	switch (edge) {
	case IRQ_TYPE_EDGE_RISING:
		type = "rising";
		break;

	case IRQ_TYPE_EDGE_FALLING:
		type = "falling";
		break;

	case IRQ_TYPE_EDGE_BOTH:
		type = "both";
		break;

	case IRQ_TYPE_EDGE_NONE:
	default:
		break;
	}

	if (!type)
		return -ENOTSUP;

	err = asprintf(&buffer, SYSFS_GPIO_PATH "/gpio%u/edge",
			gpio->num);
	if (err < 0)
		return -errno;

	fd = open(buffer, O_WRONLY);
	if (fd < 0) {
		free(buffer);
		return -errno;
	}

	free(buffer);

	err = dprintf(fd, "%s", type);
	if (err < 0) {
		err = -errno;
		close(fd);
		return err;
	}

	close(fd);

	err = asprintf(&buffer, SYSFS_GPIO_PATH "/gpio%u/value",
			gpio->num);
	if (err < 0)
		return -errno;

	fd = open(buffer, O_RDONLY);
	if (fd < 0) {
		err = -errno;
		free(buffer);
		return err;
	}

	free(buffer);

	gpio->poll.events = G_IO_ERR;
	gpio->poll.fd = fd;

	return 0;
}

struct gpio_chip {
	GSource source;

	guint base;
	struct gpio_desc **gpios;
	guint gpio_map[sizeof(enum gpio)];

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
	struct gpio_chip *chip = (struct gpio_chip *)source;
	guint i;

	for (i = 0; i < G_N_ELEMENTS(gpio_list); i++) {
		struct gpio_desc *gpio = chip->gpios[i];
		if (gpio->poll.revents & G_IO_ERR)
			return TRUE;
	}

	return FALSE;
}

static int gpio_handle_irq(struct gpio_desc *gpio,
		struct event_manager *events)
{
	const char *state = NULL;
	const char *name = NULL;
	struct event event;
	uint8_t value;
	ssize_t err;

	if (!gpio || !events)
		return -EINVAL;

	err = lseek(gpio->poll.fd, 0, SEEK_SET);
	if (err < 0)
		return -errno;

	err = read(gpio->poll.fd, &value, sizeof(value));
	if (err < 0)
		return -errno;

	value -= '0';

	memset(&event, 0, sizeof(event));

	switch (gpio->type) {
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
		g_debug("gpio-sysfs: GPIO#%u --> %u", gpio->type, value);
	}

	return 0;
}

static gboolean gpio_source_dispatch(GSource *source, GSourceFunc callback,
		gpointer user_data)
{
	struct gpio_chip *chip = (struct gpio_chip *)source;
	guint i;
	int err;

	for (i = 0; i < G_N_ELEMENTS(gpio_list); i++) {
		struct gpio_desc *gpio = chip->gpios[i];

		if (gpio->poll.revents & G_IO_ERR) {
			err = gpio_handle_irq(gpio, chip->events);
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
	struct gpio_chip *chip = (struct gpio_chip *)source;
	guint i;
	int err;

	for (i = 0; i < chip->num_exposed; i++) {
		err = gpio_free(chip->base + chip->exposed_gpios[i]);
		if (err < 0) {
			g_debug("gpio-sysfs: %s failed: %s", "gpio_free()",
					g_strerror(-err));
		}
	}

	for (i = 0; i < G_N_ELEMENTS(gpio_list); i++) {
		struct gpio_desc *gpio = chip->gpios[i];
		gpio_unexport(gpio);
	}

	g_free(chip->gpios);
	g_free(chip->exposed_gpios);
}

static GSourceFuncs gpio_source_funcs = {
	.prepare = gpio_source_prepare,
	.check = gpio_source_check,
	.dispatch = gpio_source_dispatch,
	.finalize = gpio_source_finalize,
};

int gpio_load_config(struct gpio_chip *chip, GKeyFile *config)
{
	GError *err = NULL;
	guint gpio;
	int i;

	if (!g_key_file_has_group(config, GPIO_GROUP)) {
		g_warning("gpio-sysfs: configuration is missing");
		return -EINVAL;
	}

	chip->base = g_key_file_get_integer(config, GPIO_GROUP, "base",
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
		chip->gpio_map[gpio_list[i].gpio] = gpio;
	}

	chip->exposed_gpios = g_key_file_get_integer_list(config, GPIO_GROUP,
					"expose", &chip->num_exposed, &err);
	if (err != NULL) {
		g_warning("gpio-sysfs: could not load expose list, no gpios will "
			  "be exposed (%s)", err->message);
		g_error_free(err);
		err = NULL;
	}

	return 0;
}

static struct gpio_desc* initialize_gpio(enum gpio type, GSource *source,
					 struct gpio_chip *chip)
{
	int gpio_num = chip->gpio_map[type];
	struct gpio_desc *gpio = NULL;
	int err;

	err = gpio_export(&gpio, chip->base + gpio_num);
	if (err < 0) {
		g_debug("gpio-sysfs: %s failed: %s", "gpio_export()",
				g_strerror(-err));
		goto free;
	}

	gpio->type = type;

	err = gpio_watch(gpio, IRQ_TYPE_EDGE_BOTH);
	if (err < 0) {
		g_debug("gpio-sysfs: %s failed: %s", "gpio_watch()",
				g_strerror(-err));
		goto free;
	}

	g_debug("gpio-sysfs: GPIO#%u exported, watching...",
			chip->base + gpio_num);
	g_source_add_poll(source, &gpio->poll);

	return gpio;

free:
	gpio_unexport(gpio);
	return NULL;

}

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
	gpio_load_config(chip, config);

	chip->gpios = g_new0(struct gpio_desc *, G_N_ELEMENTS(gpio_list));
	if (!chip->gpios) {
		err = -ENOMEM;
		goto unref;
	}

	for (i = 0; i < G_N_ELEMENTS(gpio_list); i++) {
		chip->gpios[i] = initialize_gpio(gpio_list[i].gpio, source,
						 chip);
		if (chip->gpios[i] == NULL) {
			g_debug("could not initialize gpio %s",
				gpio_list[i].name);
		}
	}

	for (i = 0; i < chip->num_exposed; i++) {
		err = gpio_request(chip->base + chip->exposed_gpios[i]);
		if ((err < 0) && (err != -EBUSY)) {
			g_debug("gpio-sysfs: %s failed: %s", "gpio_request",
					g_strerror(-err));
		}
	}

	*chipp = chip;
	return 0;

unref:
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
	return chip ? chip->num_exposed : -EINVAL;
}

int gpio_chip_direction_input(struct gpio_chip *chip, unsigned int gpio)
{
	int err;

	g_return_val_if_fail(chip != NULL, -EINVAL);
	g_return_val_if_fail(gpio < chip->num_exposed, -EINVAL);

	err = gpio_direction_input(chip->base + chip->exposed_gpios[gpio]);
	if (err < 0)
		return err;

	return 0;
}

int gpio_chip_direction_output(struct gpio_chip *chip, unsigned int gpio,
		int value)
{
	int err;

	g_return_val_if_fail(chip != NULL, -EINVAL);
	g_return_val_if_fail(gpio < chip->num_exposed, -EINVAL);

	err = gpio_direction_output(chip->base + chip->exposed_gpios[gpio],
				    value);
	if (err < 0)
		return err;

	return 0;
}

int gpio_chip_set_value(struct gpio_chip *chip, unsigned int gpio, int value)
{
	int err;

	g_return_val_if_fail(chip != NULL, -EINVAL);
	g_return_val_if_fail(gpio < chip->num_exposed, -EINVAL);

	err = gpio_set_value(chip->base + chip->exposed_gpios[gpio], value);
	if (err < 0)
		return err;

	return 0;
}

int gpio_chip_get_value(struct gpio_chip *chip, unsigned int gpio)
{
	int err;

	g_return_val_if_fail(chip != NULL, -EINVAL);
	g_return_val_if_fail(gpio < chip->num_exposed, -EINVAL);

	err = gpio_get_value(chip->base + chip->exposed_gpios[gpio]);
	if (err < 0)
		return err;

	return !!err;
}
