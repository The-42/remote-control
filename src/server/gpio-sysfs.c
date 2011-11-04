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

#define _GNU_SOURCE
#include <fcntl.h>
#include <glib.h>
#include <unistd.h>

#include "remote-control-stub.h"
#include "remote-control.h"

#define SYSFS_PATH "/sys"

/* FIXME: don't hard-code this */
#define GPIO_START 248

#define IRQ_TYPE_EDGE_NONE    0
#define IRQ_TYPE_EDGE_RISING  (1 << 0)
#define IRQ_TYPE_EDGE_FALLING (1 << 1)
#define IRQ_TYPE_EDGE_BOTH    (IRQ_TYPE_EDGE_RISING | IRQ_TYPE_EDGE_FALLING)

static struct gpio_map {
	unsigned int offset;
	unsigned int edge;
	enum gpio type;
} gpio_list[] = {
	{ GPIO_START + 0, IRQ_TYPE_EDGE_BOTH, GPIO_HANDSET   },
	{ GPIO_START + 1, IRQ_TYPE_EDGE_BOTH, GPIO_SMARTCARD },
};

struct gpio_desc {
	GPollFD poll;
	enum gpio type;
	guint num;
};

static int gpio_sysfs_export(unsigned int gpio)
{
	int err;
	int fd;

	fd = open(SYSFS_PATH "/class/gpio/export", O_WRONLY);
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

static int gpio_sysfs_unexport(unsigned int gpio)
{
	int err;
	int fd;

	fd = open(SYSFS_PATH "/class/gpio/unexport", O_WRONLY);
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

static int gpio_export(struct gpio_desc **gpiop, guint num)
{
	struct gpio_desc *gpio;
	int err;

	gpio = g_new0(struct gpio_desc, 1);
	if (!gpio)
		return -ENOMEM;

	gpio->poll.fd = -1;
	gpio->num = num;

	err = gpio_sysfs_export(num);
	if (err < 0) {
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

	err = gpio_sysfs_unexport(gpio->num);
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

	err = asprintf(&buffer, SYSFS_PATH "/class/gpio/gpio%u/edge",
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

	err = asprintf(&buffer, SYSFS_PATH "/class/gpio/gpio%u/value",
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

struct gpio_source {
	GSource source;

	struct gpio_desc **gpios;
	guint num_gpios;

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
	struct gpio_source *priv = (struct gpio_source *)source;
	guint i;

	for (i = 0; i < priv->num_gpios; i++) {
		struct gpio_desc *gpio = priv->gpios[i];
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
		event_manager_report(events, &event);
	} else {
		g_debug("gpio-sysfs: GPIO#%u --> %u", gpio->type, value);
	}

	return 0;
}

static gboolean gpio_source_dispatch(GSource *source, GSourceFunc callback,
		gpointer user_data)
{
	struct gpio_source *priv = (struct gpio_source *)source;
	guint i;
	int err;

	for (i = 0; i < priv->num_gpios; i++) {
		struct gpio_desc *gpio = priv->gpios[i];

		if (gpio->poll.revents & G_IO_ERR) {
			err = gpio_handle_irq(gpio, priv->events);
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
	struct gpio_source *priv = (struct gpio_source *)source;
	guint i;

	for (i = 0; i < priv->num_gpios; i++) {
		struct gpio_desc *gpio = priv->gpios[i];
		gpio_unexport(gpio);
	}

	g_free(priv->gpios);
}

static GSourceFuncs gpio_source_funcs = {
	.prepare = gpio_source_prepare,
	.check = gpio_source_check,
	.dispatch = gpio_source_dispatch,
	.finalize = gpio_source_finalize,
};

GSource *gpio_source_new(struct event_manager *events)
{
	struct gpio_source *priv;
	GSource *source;
	guint i;

	source = g_source_new(&gpio_source_funcs, sizeof(*priv));
	if (!source)
		return NULL;

	priv = (struct gpio_source *)source;
	priv->events = events;

	priv->num_gpios = G_N_ELEMENTS(gpio_list);

	priv->gpios = g_new0(struct gpio_desc *, priv->num_gpios);
	if (!priv->gpios)
		goto unref;

	for (i = 0; i < G_N_ELEMENTS(gpio_list); i++) {
		struct gpio_desc *gpio = NULL;
		int err;

		err = gpio_export(&gpio, gpio_list[i].offset);
		if (err < 0) {
			g_debug("gpio-sysfs: %s failed: %s", "gpio_export()",
					g_strerror(-err));
			goto free;
		}

		gpio->type = gpio_list[i].type;
		priv->gpios[i] = gpio;

		err = gpio_watch(gpio, gpio_list[i].edge);
		if (err < 0) {
			g_debug("gpio-sysfs: %s failed: %s", "gpio_watch()",
					g_strerror(-err));
			goto free;
		}

		g_debug("gpio-sysfs: GPIO#%u exported, watching...",
				gpio_list[i].offset);
		g_source_add_poll(source, &gpio->poll);
	}

	return source;

free:
	for (i = 0; i < priv->num_gpios; i++) {
		struct gpio_desc *gpio = priv->gpios[i];
		gpio_unexport(gpio);
	}

	g_free(priv->gpios);
unref:
	g_source_unref(source);
	return NULL;
}
