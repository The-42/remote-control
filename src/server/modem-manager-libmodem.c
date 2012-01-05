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
	enum modem_state next_state;
	enum modem_state state;
	GMutex *lock;
	GCond *cond;
	GThread *thread;
	gchar *number;
	gboolean done;
	gboolean ack;
	int wakeup[2];
	unsigned long flags;
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

	if (state == manager->state) {
		g_debug("modem-manager-libmodem: already in state %d",
				state);
		return 0;
	}

	manager->next_state = state;

	if (wakeup) {
		int err = modem_manager_wakeup(manager);
		if (err < 0) {
			g_debug("wakeup(): %s", g_strerror(-err));
			return err;
		}

		g_mutex_lock(manager->lock);

		while (manager->state != state)
			g_cond_wait(manager->cond, manager->lock);

		manager->ack = TRUE;
		g_cond_signal(manager->cond);
		g_mutex_unlock(manager->lock);
	} else {
		/* nothing to do, transition immediately */
		manager->state = state;
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

			g_debug("modem-libmodem: call ended");
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
		g_debug("modem-libmodem: DLE: %02x, %c", (unsigned char)c, display);
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

static int modem_manager_process_incoming(struct modem_manager *manager,
		enum modem_state *next_state)
{
	int err;

	g_return_val_if_fail(manager != NULL, -EINVAL);

	err = modem_accept(manager->modem);
	if (err < 0)
		return err;

	if ((manager->flags & MODEM_FLAGS_DIRECT) == 0) {
		err = modem_manager_setup_call(manager);
		if (err < 0) {
			modem_terminate(manager->modem);
			return err;
		}
	}

	if (next_state)
		*next_state = MODEM_STATE_ACTIVE;

	return 0;
}

static int modem_manager_process_outgoing(struct modem_manager *manager,
		enum modem_state *next_state)
{
	int err;

	g_return_val_if_fail(manager != NULL, -EINVAL);

	err = modem_call(manager->modem, manager->number);
	if (err < 0)
		return err;

	if ((manager->flags & MODEM_FLAGS_DIRECT) == 0) {
		err = modem_manager_setup_call(manager);
		if (err < 0) {
			modem_terminate(manager->modem);
			return err;
		}
	}

	if (next_state)
		*next_state = MODEM_STATE_ACTIVE;

	return 0;
}

static int modem_manager_process_terminate(struct modem_manager *manager,
		enum modem_state *next_state)
{
	int err;

	g_return_val_if_fail(manager != NULL, -EINVAL);

	err = modem_terminate(manager->modem);
	if (err < 0) {
		g_debug("%s(): modem_terminate(): %s", __func__,
				g_strerror(-err));
		return err;
	}

	modem_call_free(manager->call);
	manager->call = NULL;

	if (next_state)
		*next_state = MODEM_STATE_IDLE;

	return 0;
}

static int modem_manager_process_idle(struct modem_manager *manager)
{
	char buf[16];
	int err;

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

		g_debug("modem-libmodem: incoming call...");

		memset(&event, 0, sizeof(event));
		event.source = EVENT_SOURCE_MODEM;
		event.modem.state = EVENT_MODEM_STATE_RINGING;
		event_manager_report(events, &event);
	}

	return 0;
}

static int handle_modem(struct modem_manager *manager)
{
	int err;

	switch (manager->state) {
	case MODEM_STATE_ACTIVE:
		if ((manager->flags & MODEM_FLAGS_DIRECT) == 0) {
			err = modem_call_process(manager->call);
			if (err < 0) {
				g_warning("modem_call_process(): %s",
						g_strerror(-err));
			}
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
	enum modem_state next = manager->next_state;
	char data = 0;
	int err;

	err = read(manager->wakeup[0], &data, sizeof(data));
	if (err < 0)
		g_debug("%s(): read(): %s", __func__, g_strerror(errno));

	switch (manager->next_state) {
	case MODEM_STATE_INCOMING:
		err = modem_manager_process_incoming(manager, &next);
		if (err < 0) {
			g_warning("modem_manager_process_incoming(): %s",
					g_strerror(-err));
		}
		break;

	case MODEM_STATE_OUTGOING:
		err = modem_manager_process_outgoing(manager, &next);
		if (err < 0) {
			g_warning("modem_manager_process_outgoing(): %s",
					g_strerror(-err));
		}
		break;

	case MODEM_STATE_TERMINATE:
		err = modem_manager_process_terminate(manager, &next);
		if (err < 0) {
			g_warning("modem_manager_process_terminate(): %s",
					g_strerror(-err));
		}
		break;

	default:
		/* nothing to do, this should not happen */
		break;
	}

	g_mutex_lock(manager->lock);
	manager->state = manager->next_state;
	manager->ack = FALSE;
	g_cond_signal(manager->cond);
	g_mutex_unlock(manager->lock);

	g_thread_yield();

	g_mutex_lock(manager->lock);

	while (!manager->ack)
		g_cond_wait(manager->cond, manager->lock);

	g_mutex_unlock(manager->lock);

	if (next != manager->state) {
		g_mutex_lock(manager->lock);
		modem_manager_change_state(manager, next, FALSE);
		g_mutex_unlock(manager->lock);
	}

	return 0;
}

/*
 * TODO: Get rid of the modem_manager_thread() by moving the code out into
 *       a GSource that can be integrated with the GLib main loop.
 */
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

		if (manager->done)
			break;

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

static int modem_manager_open(struct modem_manager *manager)
{
	static const struct modem_desc {
		const char *device;
		unsigned long flags;
		unsigned int vls;
		unsigned int atl;
	} modem_table[] = {
		{ "/dev/ttyACM0", 0, 1, 0 },
		{ "/dev/ttyS0", MODEM_FLAGS_DIRECT, 13, 3 },
	};
	int ret = -ENODEV;
	guint i;

	for (i = 0; i < G_N_ELEMENTS(modem_table); i++) {
		const struct modem_desc *desc = &modem_table[i];
		struct modem_params params;
		int err;

		err = modem_open(&manager->modem, desc->device);
		if (err < 0)
			continue;

		g_debug("modem-libmodem: using device %s", desc->device);

		memset(&params, 0, sizeof(params));

		err = modem_get_params(manager->modem, &params);
		if (err < 0) {
			modem_close(manager->modem);
			return err;
		}

		params.flags |= desc->flags;
		params.vls = desc->vls;
		params.atl = desc->atl;

		err = modem_set_params(manager->modem, &params);
		if (err < 0)
			modem_close(manager->modem);

		manager->flags = desc->flags;
		ret = err;
		break;
	}

	return ret;
}

static int modem_manager_logv(enum modem_log_level level, const char *fmt,
			      va_list ap)
{
	gchar *message = g_strdup_vprintf(fmt, ap);
	int len = strlen(message);

	if (message[len] == '\n')
		message[len] = '\0';

	g_debug("modem-libmodem: %s", message);
	g_free(message);
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

	manager->lock = g_mutex_new();
	if (!manager->lock) {
		err = -ENOMEM;
		goto free;
	}

	manager->cond = g_cond_new();
	if (!manager->cond) {
		err = -ENOMEM;
		goto free_lock;
	}

	err = pipe2(manager->wakeup, O_NONBLOCK | O_CLOEXEC);
	if (err < 0) {
		err = -errno;
		goto free_cond;
	}

	manager->rc = rpc_server_priv(server);
	manager->state = MODEM_STATE_IDLE;
	manager->done = FALSE;
	manager->ack = FALSE;

	modem_set_logv_func(modem_manager_logv);

	err = modem_manager_open(manager);
	if (err < 0) {
		if (err != -ENODEV)
			goto close_pipe;
	} else {
		manager->thread = g_thread_create(modem_manager_thread,
				manager, TRUE, NULL);
		if (!manager->thread) {
			err = -ENOMEM;
			goto close;
		}
	}

	*managerp = manager;
	return 0;

close:
	modem_close(manager->modem);
close_pipe:
	close(manager->wakeup[0]);
	close(manager->wakeup[1]);
free_cond:
	g_cond_free(manager->cond);
free_lock:
	g_mutex_free(manager->lock);
free:
	free(manager);
	return err;
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

	g_cond_free(manager->cond);
	g_mutex_free(manager->lock);

	modem_call_free(manager->call);
	modem_close(manager->modem);
	g_free(manager->number);
	free(manager);

	return 0;
}

int modem_manager_call(struct modem_manager *manager, const char *number)
{
	g_return_val_if_fail(manager != NULL, -EINVAL);

	if (!manager->modem)
		return -ENODEV;

	g_free(manager->number);
	manager->number = g_strdup(number);

	modem_manager_change_state(manager, MODEM_STATE_OUTGOING, TRUE);

	return 0;
}

int modem_manager_accept(struct modem_manager *manager)
{
	g_return_val_if_fail(manager != NULL, -EINVAL);

	if (!manager->modem)
		return -ENODEV;

	modem_manager_change_state(manager, MODEM_STATE_INCOMING, TRUE);

	return 0;
}

int modem_manager_terminate(struct modem_manager *manager)
{
	g_return_val_if_fail(manager != NULL, -EINVAL);

	if (!manager->modem)
		return -ENODEV;

	if (manager->state == MODEM_STATE_IDLE) {
		g_debug("modem-libmodem: no call to terminate");
		return -ENOTCONN;
	}

	modem_manager_change_state(manager, MODEM_STATE_TERMINATE, TRUE);

	return 0;
}

int modem_manager_get_state(struct modem_manager *manager, enum modem_state *statep)
{
	g_return_val_if_fail((manager != NULL) && (statep != NULL), -EINVAL);

	if (!manager->modem)
		return -ENODEV;

	*statep = manager->state;
	return 0;
}
