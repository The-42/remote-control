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

#include <modem.h>

#include "remote-control-stub.h"
#include "remote-control.h"

struct modem_manager {
	struct remote_control *rc;
	struct modem *modem;
	struct modem_call *call;
	enum modem_state state;
	GThread *thread;
	gboolean done;
};

static gpointer modem_manager_thread(gpointer data)
{
	struct remote_control *rc = data;
	struct modem_manager *manager;
	struct event_manager *events;
	char buf[16];
	int err;

	manager = remote_control_get_modem_manager(rc);
	events = remote_control_get_event_manager(rc);

	while (!manager->done) {
		if (manager->state == MODEM_STATE_ACTIVE) {
			err = modem_call_process(manager->call);
			if (err < 0) {
				g_warning("modem_call_process(): %s",
						strerror(-err));
				continue;
			}
		} else {
			err = modem_recv(manager->modem, buf, sizeof(buf), 500);
			if (err < 0) {
				if (err == -ETIMEDOUT)
					continue;

				g_warning("modem_recv(): %s", strerror(-err));
				continue;
			}

			if (strcmp(buf, "RING") == 0) {
				struct event event;

				g_debug("%s(): incoming call...", __func__);
				manager->state = MODEM_STATE_INCOMING;

				memset(&event, 0, sizeof(event));
				event.source = EVENT_SOURCE_MODEM;
				event.modem.state = EVENT_MODEM_STATE_RINGING;
				event_manager_report(events, &event);
			}
		}
	}

	return NULL;
}

static int unshield_callback(char c, void *data)
{
	char display = g_ascii_isprint(c) ? c : '.';
	struct remote_control *rc = data;
	struct modem_manager *manager;
	struct event_manager *events;

	manager = remote_control_get_modem_manager(rc);
	events = remote_control_get_event_manager(rc);

	switch (c) {
	case 'b':
		if (manager->state == MODEM_STATE_ACTIVE) {
			struct event event;

			g_debug("call ended, hanging up");
			modem_manager_terminate(manager);

			memset(&event, 0, sizeof(event));
			event.source = EVENT_SOURCE_MODEM;
			event.modem.state = EVENT_MODEM_STATE_DISCONNECTED;
			event_manager_report(events, &event);
		}

		return -ENODATA;

	case 'u':
		break;

	default:
		g_debug("DLE: %02x, %c", (unsigned char)c, display);
		break;
	}

	return 0;
}

static int modem_manager_setup_call(struct modem_manager *manager)
{
	int err;

	err = modem_call_create(&manager->call, manager->modem);
	if (err < 0)
		return err;

	err = modem_call_set_unshield_callback(manager->call,
			unshield_callback, manager);
	if (err < 0) {
		modem_call_free(manager->call);
		manager->call = NULL;
		return err;
	}

	return 0;
}

int modem_manager_create(struct modem_manager **managerp, struct rpc_server *server)
{
	struct modem_manager *manager;
	int err;

	g_return_val_if_fail((managerp != NULL) && (server != NULL), -EINVAL);

	manager = calloc(1, sizeof(*manager));
	if (!manager)
		return -ENOMEM;

	manager->rc = rpc_server_priv(server);
	manager->state = MODEM_STATE_IDLE;
	manager->done = FALSE;

	err = modem_open(&manager->modem, "/dev/ttyACM0");
	if (err < 0) {
		if (err != -ENOENT) {
			free(manager);
			return err;
		}
	} else {
		manager->thread = g_thread_create(modem_manager_thread,
				manager, TRUE, NULL);
		if (!manager->thread) {
			modem_close(manager->modem);
			free(manager);
			return -ENOMEM;
		}
	}

	*managerp = manager;
	return 0;
}

int modem_manager_free(struct modem_manager *manager)
{
	g_return_val_if_fail(manager != NULL, -EINVAL);

	if (manager->thread) {
		manager->done = TRUE;
		g_thread_join(manager->thread);
	}

	modem_call_free(manager->call);
	modem_close(manager->modem);
	free(manager);

	return 0;
}

int modem_manager_call(struct modem_manager *manager, const char *number)
{
	int err;

	g_return_val_if_fail(manager != NULL, -EINVAL);
	g_return_val_if_fail(manager->modem != NULL, -ENODEV);

	err = modem_call(manager->modem, number);
	if (err < 0)
		return err;

	err = modem_manager_setup_call(manager);
	if (err < 0) {
		modem_terminate(manager->modem);
		return err;
	}

	manager->state = MODEM_STATE_ACTIVE;

	return 0;
}

int modem_manager_accept(struct modem_manager *manager)
{
	int err;

	g_return_val_if_fail(manager != NULL, -EINVAL);
	g_return_val_if_fail(manager->modem != NULL, -ENODEV);

	err = modem_accept(manager->modem);
	if (err < 0)
		return err;

	err = modem_manager_setup_call(manager);
	if (err < 0) {
		modem_terminate(manager->modem);
		return err;
	}

	manager->state = MODEM_STATE_ACTIVE;

	return 0;
}

int modem_manager_terminate(struct modem_manager *manager)
{
	int err;

	g_return_val_if_fail(manager != NULL, -EINVAL);
	g_return_val_if_fail(manager->modem != NULL, -ENODEV);

	err = modem_terminate(manager->modem);
	if (err < 0)
		return err;

	manager->state = MODEM_STATE_IDLE;
	modem_call_free(manager->call);
	manager->call = NULL;

	return 0;
}

int modem_manager_get_state(struct modem_manager *manager, enum modem_state *statep)
{
	g_return_val_if_fail((manager != NULL) && (statep != NULL), -EINVAL);
	g_return_val_if_fail(manager->modem != NULL, -ENODEV);

	*statep = manager->state;
	return 0;
}
