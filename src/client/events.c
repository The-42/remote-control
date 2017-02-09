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

#include "remote-control-stub.h"
#include "remote-control.h"

remote_public
int remote_register_event_handler(struct remote_client *client,
                enum remote_event queue, remote_event_handler_t handler,
                void *data)
{
#if 0
	struct event_handler *eh;
	struct list_head *head;

	if (!client || !handler || (queue < 0) || (queue >= REMOTE_EVENT_MAX))
		return -EINVAL;

	head = &client->handlers[queue];

	eh = malloc(sizeof(*eh));
	if (!eh)
		return -ENOMEM;

	memset(eh, 0, sizeof(*eh));
	eh->handler = handler;
	list_init(&eh->list);
	eh->data = data;

	list_add_tail(&eh->list, head);

	return 0;
#else
	return -ENOSYS;
#endif
}

remote_public
int remote_unregister_event_handler(struct remote_client *client,
                enum remote_event queue, remote_event_handler_t handler)
{
#if 0
	struct list_head *node, *temp;
	struct event_handler *eh;
	struct list_head *head;

	if (!client || (queue < 0) || (queue >= REMOTE_EVENT_MAX))
		return -EINVAL;

	head = &client->handlers[queue];

	list_for_each_safe(node, temp, head) {
		eh = list_entry(node, struct event_handler, list);
		if (eh->handler == handler) {
			list_del_init(&eh->list);
			free(eh);
		}
	}

	return 0;
#else
	return -ENOSYS;
#endif
}

static int remote_call_events(struct remote_client *client,
		enum remote_event queue, uint32_t type)
{
#if 0
	struct event_handler *eh;
	struct list_head *node;
	struct list_head *head;

	if (!client || (queue < 0) || (queue >= REMOTE_EVENT_MAX))
		return -EINVAL;

	head = &client->handlers[queue];

	list_for_each(node, head) {
		eh = list_entry(node, struct event_handler, list);
		if (!eh->handler) {
			fprintf(stderr, "no event handler defined!\n");
			continue;
		}

		eh->handler(type, eh->data);
	}

	return 0;
#else
	return -ENOSYS;
#endif
}

void RPC_IMPL(card_event)(void *priv, uint32_t type)
{
	remote_call_events(priv, REMOTE_EVENT_CARD, type);
}

void RPC_IMPL(modem_event)(void *priv, uint32_t type)
{
	remote_call_events(priv, REMOTE_EVENT_MODEM, type);
}

void RPC_IMPL(voip_event)(void *priv, uint32_t type)
{
	remote_call_events(priv, REMOTE_EVENT_VOIP, type);
}
