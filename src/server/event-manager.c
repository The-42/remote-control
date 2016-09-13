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

struct event_manager {
	enum event_smartcard_state smartcard_state;
	enum event_hook_state hook_state;
};

int event_manager_create(struct event_manager **managerp)
{
	struct event_manager *manager;

	if (!managerp)
		return -EINVAL;

	manager = g_new0(struct event_manager, 1);
	if (!manager)
		return -ENOMEM;

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
	int ret = 0;

	g_debug("> %s(manager=%p, event=%p)", __func__, manager, event);

	switch (event->source) {
	case EVENT_SOURCE_SMARTCARD:
		manager->smartcard_state = event->smartcard.state;
		break;

	case EVENT_SOURCE_HOOK:
		manager->hook_state = event->hook.state;
		break;

	default:
		ret = -ENXIO;
		break;
	}

	return ret;
}

int event_manager_get_source_state(struct event_manager *manager, struct event *event)
{
	int err = 0;

	if (!manager || !event)
		return -EINVAL;

	switch (event->source) {
	case EVENT_SOURCE_SMARTCARD:
		event->smartcard.state = manager->smartcard_state;
		break;

	case EVENT_SOURCE_HOOK:
		event->hook.state = manager->hook_state;
		break;

	default:
		err = -ENOSYS;
		break;
	}

	return err;
}
