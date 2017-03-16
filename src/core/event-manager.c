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

#include "remote-control.h"

struct event_manager {
	enum event_voip_state voip_state;
	enum event_smartcard_state smartcard_state;
	enum event_hook_state hook_state;
	enum event_modem_state modem_state;

	GQueue *handset_events;
	GList *callbacks;
};

struct event_callback {
	event_manager_event_cb func_cb;
	void *data;
	void *owner;
};

int event_manager_create(struct event_manager **managerp)
{
	struct event_manager *manager;
	int err = 0;

	if (!managerp)
		return -EINVAL;

	manager = g_new0(struct event_manager, 1);
	if (!manager)
		return -ENOMEM;

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

	g_list_free_full(manager->callbacks, g_free);
	g_queue_free(manager->handset_events);
	g_free(manager);
	return 0;
}

int event_manager_report(struct event_manager *manager, struct event *event)
{
	gpointer item;
	int ret = 0;
	GList *cb;

	if (!manager || !event)
		return -EINVAL;

	for (cb = manager->callbacks; cb != NULL; cb = cb->next) {
		struct event_callback *ecb = cb->data;

		if (!ecb)
			continue;

		ret = ecb->func_cb(ecb->data, event);
		if (ret) {
			g_debug("%s: callback failed: %s", __func__,
				strerror(-ret));
		}
	}

	switch (event->source) {
	case EVENT_SOURCE_MODEM:
		manager->modem_state = event->modem.state;
		break;

	case EVENT_SOURCE_VOIP:
		manager->voip_state = event->voip.state;
		break;

	case EVENT_SOURCE_SMARTCARD:
		g_debug("SMARTCARD: %d -> %d (%d)", manager->smartcard_state,
				event->smartcard.state, ret);
		manager->smartcard_state = event->smartcard.state;
		break;

	case EVENT_SOURCE_HOOK:
		g_debug("HOOK: %d -> %d (%d)", manager->hook_state,
				event->hook.state, ret);
		manager->hook_state = event->hook.state;
		break;

	case EVENT_SOURCE_HANDSET:
		item = g_new(struct event_handset, 1);
		if (!item) {
			ret = -ENOMEM;
			break;
		}

		memcpy(item, &event->handset, sizeof(event->handset));
		g_queue_push_tail(manager->handset_events, item);
		break;

	default:
		g_debug("Unknown event: %d (%d)", event->source, ret);
		ret = -ENXIO;
		break;
	}

	return ret;
}

int event_manager_get_source_state(struct event_manager *manager, struct event *event)
{
	gpointer item;
	int err = 0;

	if (!manager || !event)
		return -EINVAL;

	switch (event->source) {
	case EVENT_SOURCE_MODEM:
		event->modem.state = manager->modem_state;
		break;

	case EVENT_SOURCE_VOIP:
		event->voip.state = manager->voip_state;
		break;

	case EVENT_SOURCE_SMARTCARD:
		event->smartcard.state = manager->smartcard_state;
		break;

	case EVENT_SOURCE_HOOK:
		event->hook.state = manager->hook_state;
		break;

	case EVENT_SOURCE_HANDSET:
		item = g_queue_pop_head(manager->handset_events);
		if (item) {
			memcpy(&event->handset, item, sizeof(event->handset));
			g_free(item);
		} else {
			err = -ENODATA;
		}
		break;

	default:
		err = -ENOSYS;
		break;
	}

	return err;
}

int event_manager_set_event_cb(struct event_manager *manager,
		event_manager_event_cb callback, void *data, void *owner_ref)
{
	struct event_callback *ecb;
	GList *cb;

	g_return_val_if_fail(manager != NULL, -EINVAL);

	for (cb = manager->callbacks; cb != NULL; cb = cb->next) {
		ecb = cb->data;
		if (ecb && ecb->owner == owner_ref) {
			manager->callbacks = g_list_remove(manager->callbacks,
				ecb);
			g_free(ecb);
		}
	}

	ecb = g_new0(struct event_callback, 1);

	ecb->owner = owner_ref;
	ecb->data = data;
	ecb->func_cb = callback;

	manager->callbacks = g_list_insert(manager->callbacks, ecb, 0);

	return 0;
}

void *event_manager_get_event_cb_owner(struct event_manager *manager,
		event_manager_event_cb callback)
{
	struct event_callback *ecb;
	GList *cb;

	if (!manager)
		return NULL;

	for (cb = manager->callbacks; cb != NULL; cb = cb->next) {
		ecb = cb->data;
		if (ecb && ecb->func_cb == callback)
			return ecb->owner;
	}

	return NULL;
}
