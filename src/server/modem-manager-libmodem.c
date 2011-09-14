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

#define _GNU_SOURCE 1

#include "remote-control-stub.h"
#include "remote-control.h"

#include <fcntl.h>
#include <modem.h>
#include <unistd.h>

struct modem_manager {
	struct remote_control *rc;
	struct modem *modem;
	struct modem_call *call;
	enum modem_state state;
	GThread *thread;
	gchar *number;
	gboolean done;
	int wakeup[2];
};

static int modem_manager_wakeup(struct modem_manager *manager)
{
	const char data = 'W';
	int err;

	if (!manager)
		return -EINVAL;

	err = write(manager->wakeup[1], &data, sizeof(data));
	if (err < 0)
		return -errno;

	return 0;
}

static int modem_manager_change_state(struct modem_manager *manager,
		enum modem_state state, gboolean wakeup)
{
	g_return_val_if_fail(manager != NULL, -EINVAL);

	manager->state = state;

	if (wakeup) {
		int err = modem_manager_wakeup(manager);
		if (err < 0)
			g_debug("wakeup(): %s", g_strerror(-err));
	}

	return 0;
}

static int unshield_callback(char c, void *data)
{
	char display = g_ascii_isprint(c) ? c : '.';
	struct modem_manager *manager = data;
	struct event_manager *events;

	events = remote_control_get_event_manager(manager->rc);

	switch (c) {
	case 'b':
		if (manager->state == MODEM_STATE_ACTIVE) {
			struct event event;

			g_debug("call ended, hanging up");
			modem_manager_change_state(manager, MODEM_STATE_TERMINATE, TRUE);

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

static int modem_manager_process_incoming(struct modem_manager *manager)
{
	int err;

	g_debug("> %s(manager=%p)", __func__, manager);

	g_return_val_if_fail(manager != NULL, -EINVAL);

	err = modem_accept(manager->modem);
	if (err < 0)
		return err;

	err = modem_manager_setup_call(manager);
	if (err < 0) {
		modem_terminate(manager->modem);
		return err;
	}

	modem_manager_change_state(manager, MODEM_STATE_ACTIVE, FALSE);
	g_debug("< %s()", __func__);
	return 0;
}

static int modem_manager_process_outgoing(struct modem_manager *manager)
{
	int err;

	g_debug("> %s(manager=%p)", __func__, manager);

	g_return_val_if_fail(manager != NULL, -EINVAL);

	err = modem_call(manager->modem, manager->number);
	if (err < 0)
		return err;

	err = modem_manager_setup_call(manager);
	if (err < 0) {
		modem_terminate(manager->modem);
		return err;
	}

	modem_manager_change_state(manager, MODEM_STATE_ACTIVE, FALSE);
	g_debug("< %s()", __func__);
	return 0;
}

static int modem_manager_process_terminate(struct modem_manager *manager)
{
	int err;

	g_debug("> %s(manager=%p)", __func__, manager);
	g_return_val_if_fail(manager != NULL, -EINVAL);

	if (manager->call) {
		err = modem_terminate(manager->modem);
		if (err < 0) {
			g_debug("%s(): modem_terminate(): %s", __func__,
					g_strerror(-err));
			return err;
		}

		modem_call_free(manager->call);
		manager->call = NULL;
	}

	modem_manager_change_state(manager, MODEM_STATE_IDLE, FALSE);

	g_debug("< %s()", __func__);
	return 0;
}

static int modem_manager_process_idle(struct modem_manager *manager)
{
	char buf[16];
	int err;

	g_debug("> %s(manager=%p)", __func__, manager);

	g_return_val_if_fail(manager != NULL, -EINVAL);

	err = modem_recv(manager->modem, buf, sizeof(buf), 500);
	if (err < 0) {
		g_debug("%s(): modem_recv(): %s", __func__, g_strerror(-err));
		return err;
	}

	if (strcmp(buf, "RING") == 0) {
		struct event_manager *events;
		struct event event;

		events = remote_control_get_event_manager(manager->rc);

		g_debug("%s(): incoming call...", __func__);

		memset(&event, 0, sizeof(event));
		event.source = EVENT_SOURCE_MODEM;
		event.modem.state = EVENT_MODEM_STATE_RINGING;
		event_manager_report(events, &event);
	}

	g_debug("< %s()", __func__);
	return 0;
}

static int handle_modem(struct modem_manager *manager)
{
	int err;

	switch (manager->state) {
	case MODEM_STATE_ACTIVE:
		err = modem_call_process(manager->call);
		if (err < 0) {
			g_warning("modem_call_process(): %s",
					g_strerror(-err));
		}
		break;

	case MODEM_STATE_IDLE:
		err = modem_manager_process_idle(manager);
		if (err < 0) {
			g_warning("modem_manager_process_idle(): %s",
					g_strerror(-err));
		}
		break;

	default:
		/* nothing to do, this should not happen */
		break;
	}

	return 0;
}

static int handle_wakeup(struct modem_manager *manager)
{
	char data = 0;
	int err;

	err = read(manager->wakeup[0], &data, sizeof(data));
	if (err < 0)
		g_debug("%s(): read(): %s", __func__, g_strerror(errno));

	switch (manager->state) {
	case MODEM_STATE_INCOMING:
		err = modem_manager_process_incoming(manager);
		if (err < 0) {
			g_warning("modem_manager_process_incoming(): %s",
					g_strerror(-err));
		}
		break;

	case MODEM_STATE_OUTGOING:
		err = modem_manager_process_outgoing(manager);
		if (err < 0) {
			g_warning("modem_manager_process_outgoing(): %s",
					g_strerror(-err));
		}
		break;

	case MODEM_STATE_TERMINATE:
		err = modem_manager_process_terminate(manager);
		if (err < 0) {
			g_warning("modem_manager_process_terminate(): %s",
					g_strerror(-err));
		}
		break;

	default:
		/* nothing to do, this should not happen */
		break;
	}

	return 0;
}

static gpointer modem_manager_thread(gpointer data)
{
	struct modem_manager *manager = data;
	gint err;

	while (!manager->done) {
		GPollFD fds[2];

		fds[0].fd = modem_get_fd(manager->modem);
		fds[0].events = G_IO_IN;
		fds[1].fd = manager->wakeup[0];
		fds[1].events = G_IO_IN;

		err = g_poll(fds, G_N_ELEMENTS(fds), -1);
		if (err <= 0) {
			if ((err == 0) || (errno == EINTR))
				continue;

			g_warning("%s(): g_poll(): %s", __func__,
					g_strerror(errno));
			break;
		}

		/* handle input from the modem */
		if (fds[0].revents & G_IO_IN) {
			err = handle_modem(manager);
			if (err < 0) {
			}
		}

		/* handle wakeup events */
		if (fds[1].revents & G_IO_IN) {
			err = handle_wakeup(manager);
			if (err < 0) {
			}
		}
	}

	return NULL;
}

int modem_manager_create(struct modem_manager **managerp, struct rpc_server *server)
{
	struct modem_manager *manager;
	int err;

	g_return_val_if_fail((managerp != NULL) && (server != NULL), -EINVAL);

	manager = calloc(1, sizeof(*manager));
	if (!manager)
		return -ENOMEM;

	err = pipe2(manager->wakeup, O_NONBLOCK | O_CLOEXEC);
	if (err < 0)
		return -errno;

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
		modem_manager_wakeup(manager);
		g_thread_join(manager->thread);
	}

	close(manager->wakeup[0]);
	close(manager->wakeup[1]);

	modem_call_free(manager->call);
	modem_close(manager->modem);
	g_free(manager->number);
	free(manager);

	return 0;
}

int modem_manager_call(struct modem_manager *manager, const char *number)
{
	g_debug("> %s(manager=%p, number=%s)", __func__, manager, number);

	g_return_val_if_fail(manager != NULL, -EINVAL);
	g_return_val_if_fail(manager->modem != NULL, -ENODEV);

	g_free(manager->number);
	manager->number = g_strdup(number);

	modem_manager_change_state(manager, MODEM_STATE_OUTGOING, TRUE);

	g_debug("< %s()", __func__);
	return 0;
}

int modem_manager_accept(struct modem_manager *manager)
{
	g_debug("> %s(manager=%p)", __func__, manager);

	g_return_val_if_fail(manager != NULL, -EINVAL);
	g_return_val_if_fail(manager->modem != NULL, -ENODEV);

	modem_manager_change_state(manager, MODEM_STATE_INCOMING, TRUE);

	g_debug("< %s()", __func__);
	return 0;
}

int modem_manager_terminate(struct modem_manager *manager)
{
	g_debug("> %s(manager=%p)", __func__, manager);

	g_return_val_if_fail(manager != NULL, -EINVAL);
	g_return_val_if_fail(manager->modem != NULL, -ENODEV);

	modem_manager_change_state(manager, MODEM_STATE_TERMINATE, TRUE);

	g_debug("< %s()", __func__);
	return 0;
}

int modem_manager_get_state(struct modem_manager *manager, enum modem_state *statep)
{
	g_return_val_if_fail((manager != NULL) && (statep != NULL), -EINVAL);
	g_return_val_if_fail(manager->modem != NULL, -ENODEV);

	*statep = manager->state;
	return 0;
}
