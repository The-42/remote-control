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

#include <fcntl.h>
#include <sys/ioctl.h>

#include <glib.h>

#ifdef HAVE_LINUX_GPIODEV_H
#include <linux/gpiodev.h>
#endif

#include "remote-control-stub.h"
#include "remote-control.h"

enum gpio {
	GPIO_HANDSET,
	GPIO_TOUCHSCREEN,
	GPIO_SMARTCARD,
};

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define BIT(x) (1 << (x))

enum gpio gpio_list[] = {
	GPIO_HANDSET,
	GPIO_SMARTCARD,
};

struct event_manager {
	GSource source;
	GPollFD fd;
	int gpiofd;

	struct rpc_server *server;
	uint32_t irq_status;

	enum event_voip_state voip_state;
	enum event_smartcard_state smartcard_state;
	enum event_hook_state hook_state;
	enum event_rfid_state rfid_state;
	enum event_modem_state modem_state;
	GQueue *handset_events;
};

static gboolean event_manager_source_prepare(GSource *source, gint *timeout)
{
	if (timeout)
		*timeout = -1;

	return FALSE;
}

static gboolean event_manager_source_check(GSource *source)
{
	struct event_manager *manager = (struct event_manager *)source;

	if (manager->fd.revents & G_IO_IN)
		return TRUE;

	return FALSE;
}

static gboolean event_manager_source_dispatch(GSource *source, GSourceFunc callback, gpointer user_data)
{
#ifdef HAVE_LINUX_GPIODEV_H
	struct event_manager *manager = (struct event_manager *)source;
	struct gpio_event gpio;
	struct event event;
	int err;

	err = read(manager->gpiofd, &gpio, sizeof(gpio));
	if (err < 0) {
		g_error("read(): %s", strerror(errno));
		return TRUE;
	}

	switch (gpio.gpio) {
	case GPIO_HANDSET:
		event.source = EVENT_SOURCE_HOOK;
		if (gpio.value) {
			g_debug("  HOOK transitioned to OFF state");
			event.hook.state = EVENT_HOOK_STATE_OFF;
		} else {
			g_debug("  HOOK transitioned to ON state");
			event.hook.state = EVENT_HOOK_STATE_ON;
		}
		break;

	case GPIO_SMARTCARD:
		event.source = EVENT_SOURCE_SMARTCARD;
		if (gpio.value) {
			g_debug("  SMARTCARD transitioned to REMOVED state");
			event.smartcard.state = EVENT_SMARTCARD_STATE_REMOVED;
		} else {
			g_debug("  SMARTCARD transitioned to INSERTED state");
			event.smartcard.state = EVENT_SMARTCARD_STATE_INSERTED;
		}
		break;

	default:
		g_debug("  unknown GPIO %u transitioned to %u",
				gpio.gpio, gpio.value);
		break;
	}

	event_manager_report(manager, &event);
#endif /* HAVE_LINUX_GPIODEV_H */

	if (callback)
		return callback(user_data);

	return TRUE;
}

static void event_manager_source_finalize(GSource *source)
{
	struct event_manager *manager = (struct event_manager *)source;

	g_queue_free(manager->handset_events);

	if (manager->gpiofd >= 0)
		close(manager->gpiofd);
}

static GSourceFuncs event_manager_source_funcs = {
	.prepare = event_manager_source_prepare,
	.check = event_manager_source_check,
	.dispatch = event_manager_source_dispatch,
	.finalize = event_manager_source_finalize,
};

int event_manager_create(struct event_manager **managerp, struct rpc_server *server)
{
	struct event_manager *manager;
	GSource *source;
#ifdef HAVE_LINUX_GPIODEV_H
	unsigned int i;
#endif
	int err = 0;

	if (!managerp) {
		err = -EINVAL;
		goto out;
	}

	source = g_source_new(&event_manager_source_funcs, sizeof(*manager));
	if (!source) {
		err = -ENOMEM;
		goto out;
	}

	manager = (struct event_manager *)source;
	manager->server = server;
	manager->gpiofd = -1;
	manager->irq_status = 0;

	manager->voip_state = EVENT_VOIP_STATE_IDLE;
	manager->smartcard_state = EVENT_SMARTCARD_STATE_REMOVED;
	manager->hook_state = EVENT_HOOK_STATE_ON;
	manager->rfid_state = EVENT_RFID_STATE_LOST;
	manager->modem_state = EVENT_MODEM_STATE_DISCONNECTED;

	manager->handset_events = g_queue_new();
	if (!manager->handset_events) {
		err = -ENOMEM;
		goto free;
	}

#ifdef HAVE_LINUX_GPIODEV_H
	manager->gpiofd = open("/dev/gpio-0", O_RDWR);
	if (manager->gpiofd < 0) {
		err = -errno;
		goto free;
	}

	for (i = 0; i < ARRAY_SIZE(gpio_list); i++) {
		struct gpio_event enable;

		enable.gpio = gpio_list[i];
		enable.value = 1;

		err = ioctl(manager->gpiofd, GPIO_IOC_ENABLE_IRQ, &enable);
		if (err < 0) {
			err = -errno;
			goto close;
		}
	}

	manager->fd.events = G_IO_IN | G_IO_HUP | G_IO_ERR;
	manager->fd.fd = manager->gpiofd;

	g_source_add_poll(source, &manager->fd);
#endif /* HAVE_LINUX_GPIODEV_H */

	*managerp = manager;
	goto out;

#ifdef HAVE_LINUX_GPIODEV_H
close:
	close(manager->gpiofd);
free:
	g_free(source);
#endif /* HAVE_LINUX_GPIODEV_H */
out:
	return err;
}

GSource *event_manager_get_source(struct event_manager *manager)
{
	return manager ? &manager->source : NULL;
}

int event_manager_report(struct event_manager *manager, struct event *event)
{
	uint32_t irq_status = 0;
	gpointer item;
	int ret = 0;

	switch (event->source) {
	case EVENT_SOURCE_MODEM:
		manager->modem_state = event->modem.state;
		irq_status |= BIT(EVENT_SOURCE_MODEM);
		break;

	case EVENT_SOURCE_IO:
		irq_status |= BIT(EVENT_SOURCE_IO);
		break;

	case EVENT_SOURCE_VOIP:
		manager->voip_state = event->voip.state;
		irq_status |= BIT(EVENT_SOURCE_VOIP);
		break;

	case EVENT_SOURCE_SMARTCARD:
		manager->smartcard_state = event->smartcard.state;
		irq_status |= BIT(EVENT_SOURCE_SMARTCARD);
		break;

	case EVENT_SOURCE_HOOK:
		manager->hook_state = event->hook.state;
		irq_status |= BIT(EVENT_SOURCE_HOOK);
		break;

	case EVENT_SOURCE_RFID:
		manager->rfid_state = event->rfid.state;
		irq_status |= BIT(EVENT_SOURCE_RFID);
		break;

	case EVENT_SOURCE_HANDSET:
		item = g_new(struct event_handset, 1);
		if (!item) {
			ret = -ENOMEM;
			break;
		}

		memcpy(item, &event->handset, sizeof(event->handset));
		g_queue_push_tail(manager->handset_events, item);
		irq_status |= BIT(EVENT_SOURCE_HANDSET);
		break;

	default:
		ret = -ENXIO;
		break;
	}

	if (irq_status != manager->irq_status) {
		ret = RPC_STUB(irq_event)(manager->server, 0);
		manager->irq_status |= irq_status;
	}

	return ret;
}

int event_manager_get_status(struct event_manager *manager, uint32_t *statusp)
{
	if (!manager || !statusp)
		return -EINVAL;

	*statusp = manager->irq_status;
	return 0;
}

int event_manager_get_source_state(struct event_manager *manager, struct event *event)
{
	uint32_t irq_status;
	gpointer item;
	int err = 0;

	if (!manager || !event)
		return -EINVAL;

	irq_status = manager->irq_status;

	switch (event->source) {
	case EVENT_SOURCE_MODEM:
		event->modem.state = manager->modem_state;
		irq_status &= ~BIT(EVENT_SOURCE_MODEM);
		break;

	case EVENT_SOURCE_VOIP:
		event->voip.state = manager->voip_state;
		irq_status &= ~BIT(EVENT_SOURCE_VOIP);
		break;

	case EVENT_SOURCE_SMARTCARD:
		event->smartcard.state = manager->smartcard_state;
		irq_status &= ~BIT(EVENT_SOURCE_SMARTCARD);
		break;

	case EVENT_SOURCE_HOOK:
		event->hook.state = manager->hook_state;
		irq_status &= ~BIT(EVENT_SOURCE_HOOK);
		break;

	case EVENT_SOURCE_RFID:
		event->rfid.state = manager->rfid_state;
		irq_status &= ~BIT(EVENT_SOURCE_RFID);
		break;

	case EVENT_SOURCE_HANDSET:
		item = g_queue_pop_head(manager->handset_events);
		if (item) {
			memcpy(&event->handset, item, sizeof(event->handset));
			g_free(item);
		} else {
			err = -ENODATA;
		}

		if (g_queue_is_empty(manager->handset_events))
			irq_status &= ~BIT(EVENT_SOURCE_HANDSET);

		break;

	default:
		err = -ENOSYS;
		break;
	}

	if (irq_status == manager->irq_status)
		err = RPC_STUB(irq_event)(manager->server, 0);
	else
		manager->irq_status = irq_status;

	return err;
}
