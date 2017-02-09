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

#include "remote-control-stub.h"
#include "remote-control.h"

#define BIT(x) (1 << (x))

struct event_manager {
	struct rpc_server *server;
	uint32_t irq_status;

	enum event_voip_state voip_state;
	enum event_smartcard_state smartcard_state;
	enum event_hook_state hook_state;
	enum event_modem_state modem_state;

	GQueue *handset_events;

	event_manager_event_cb event_cb;
	void *event_cb_data;
	void *event_cb_owner;
};

int event_manager_create(struct event_manager **managerp,
		struct rpc_server *server)
{
	struct event_manager *manager;
	int err = 0;

	if (!managerp || !server)
		return -EINVAL;

	manager = g_new0(struct event_manager, 1);
	if (!manager)
		return -ENOMEM;

	manager->server = server;
	manager->irq_status = 0;

	manager->voip_state = EVENT_VOIP_STATE_IDLE;
	manager->smartcard_state = EVENT_SMARTCARD_STATE_REMOVED;
	manager->hook_state = EVENT_HOOK_STATE_ON;
	manager->modem_state = EVENT_MODEM_STATE_DISCONNECTED;

	manager->handset_events = g_queue_new();
	if (!manager->handset_events) {
		err = -ENOMEM;
		goto free;
	}

	*managerp = manager;
	return 0;

free:
	g_queue_free(manager->handset_events);
	g_free(manager);
	return err;
}

int event_manager_free(struct event_manager *manager)
{
	if (!manager)
		return -EINVAL;

	g_queue_free(manager->handset_events);
	g_free(manager);
	return 0;
}

int event_manager_report(struct event_manager *manager, struct event *event)
{
	uint32_t irq_status = 0;
	gpointer item;
	int ret = 0;

	if (manager->event_cb)
		ret = manager->event_cb(manager->event_cb_data, event);

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
		g_debug("SMARTCARD: %d -> %d (%d)", manager->smartcard_state,
				event->smartcard.state, ret);
		manager->smartcard_state = event->smartcard.state;
		irq_status |= BIT(EVENT_SOURCE_SMARTCARD);
		break;

	case EVENT_SOURCE_HOOK:
		g_debug("HOOK: %d -> %d (%d)", manager->hook_state,
				event->hook.state, ret);
		manager->hook_state = event->hook.state;
		irq_status |= BIT(EVENT_SOURCE_HOOK);
		break;

	case EVENT_SOURCE_HANDSET:
		item = g_new(struct event_handset, 1);
		if (!item) {
			ret = -ENOMEM;
			goto out;
		}

		memcpy(item, &event->handset, sizeof(event->handset));
		g_queue_push_tail(manager->handset_events, item);
		irq_status |= BIT(EVENT_SOURCE_HANDSET);
		break;

	default:
		g_debug("Unknown event: %d (%d)", event->source, ret);
		ret = -ENXIO;
		goto out;
	}

	g_debug("  IRQ: %08x", irq_status);

	if (irq_status != manager->irq_status) {
		ret = RPC_STUB(irq_event)(manager->server, 0);
		manager->irq_status |= irq_status;
	}

out:
	g_debug("< %s() = %d", __func__, ret);
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

int event_manager_set_event_cb(struct event_manager *manager,
		event_manager_event_cb callback, void *data, void *owner_ref)
{
	g_return_val_if_fail(manager != NULL, -EINVAL);

	manager->event_cb_owner = owner_ref;
	manager->event_cb_data = data;
	manager->event_cb = callback;

	return 0;
}

void *event_manager_get_event_cb_owner(struct event_manager *manager)
{
	return manager ? manager->event_cb_owner : NULL;
}
