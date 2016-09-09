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

#define IRQ_SEND_ALWAYS 1
#define BIT(x) (1 << (x))

struct event_manager {
	struct rpc_server *server;
	uint32_t irq_status;

	enum event_smartcard_state smartcard_state;
	enum event_hook_state hook_state;
};

int event_manager_create(struct event_manager **managerp,
		struct rpc_server *server)
{
	struct event_manager *manager;

	if (!managerp || !server)
		return -EINVAL;

	manager = g_new0(struct event_manager, 1);
	if (!manager)
		return -ENOMEM;

	manager->server = server;
	manager->irq_status = 0;

	manager->smartcard_state = EVENT_SMARTCARD_STATE_REMOVED;
	manager->hook_state = EVENT_HOOK_STATE_ON;

	*managerp = manager;
	return 0;
}

int event_manager_free(struct event_manager *manager)
{
	if (!manager)
		return -EINVAL;

	g_free(manager);
	return 0;
}

int event_manager_report(struct event_manager *manager, struct event *event)
{
	uint32_t irq_status = 0;
	int ret = 0;

	g_debug("> %s(manager=%p, event=%p)", __func__, manager, event);

	switch (event->source) {
	case EVENT_SOURCE_SMARTCARD:
		manager->smartcard_state = event->smartcard.state;
		irq_status |= BIT(EVENT_SOURCE_SMARTCARD);
		break;

	case EVENT_SOURCE_HOOK:
		manager->hook_state = event->hook.state;
		irq_status |= BIT(EVENT_SOURCE_HOOK);
		break;

	default:
		ret = -ENXIO;
		goto out;
	}

	g_debug("  IRQ: %08x", irq_status);

#ifdef IRQ_SEND_ALWAYS
	/*
	 * FIXME: This is a horrible work-around for broken user-interfaces
	 *        that loose interrupts somewhere along the way. With the
	 *        default behaviour, this causes subsequent interrupts to not
	 *        be propagated to the user-interface until the original
	 *        interrupt has been cleared.
	 *
	 *        Note that the default behaviour was actually introduced at
	 *        some point to get the user-interface to behave properly
	 *        because it couldn't process interrupts fast enough.
	 *
	 *        Now it turns out that there is some other problem in the
	 *        user-interface implementation that causes interrupts to be
	 *        dropped. But instead of fixing the bug, it was decided to
	 *        work around it by sending out interrupts every time.
	 *
	 *        I protested but I was ignored.
	 */
	ret = RPC_STUB(irq_event)(manager->server, 0);
	manager->irq_status |= irq_status;
#else
	if (irq_status != manager->irq_status) {
		ret = RPC_STUB(irq_event)(manager->server, 0);
		manager->irq_status |= irq_status;
	}
#endif

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
	int err = 0;

	if (!manager || !event)
		return -EINVAL;

	irq_status = manager->irq_status;

	switch (event->source) {
	case EVENT_SOURCE_SMARTCARD:
		event->smartcard.state = manager->smartcard_state;
		irq_status &= ~BIT(EVENT_SOURCE_SMARTCARD);
		break;

	case EVENT_SOURCE_HOOK:
		event->hook.state = manager->hook_state;
		irq_status &= ~BIT(EVENT_SOURCE_HOOK);
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
