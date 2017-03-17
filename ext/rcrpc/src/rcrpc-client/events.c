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
#include "rcrpc-client.h"

remote_public
int remote_register_event_handler(struct remote_client *client,
                enum remote_event queue, remote_event_handler_t handler,
                void *data)
{
	return -ENOSYS;
}

remote_public
int remote_unregister_event_handler(struct remote_client *client,
                enum remote_event queue, remote_event_handler_t handler)
{
	return -ENOSYS;
}

static int remote_call_events(struct remote_client *client,
		enum remote_event queue, uint32_t type)
{
	return -ENOSYS;
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
