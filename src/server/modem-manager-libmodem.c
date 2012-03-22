/*
 * Copyright (C) 2011-2012 Avionic Design GmbH
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

#include <ctype.h>
#include <fcntl.h>
#include <modem.h>
#include <unistd.h>

struct modem_desc {
	char *device;
	unsigned long flags;
	unsigned int vls;
	unsigned int atl;
};

struct modem_manager {
	GSource source;
	GPollFD poll;
	struct remote_control *rc;
	struct modem *modem;
	struct modem_call *call;
	enum modem_state state;
	unsigned long flags;
	gchar *number;
};

static int modem_ring(struct modem *modem, void *data)
{
	struct modem_manager *manager = data;
	struct event_manager *events;
	struct event event;
	int err = 0;

	if ((manager->state != MODEM_STATE_RINGING) &&
	    (manager->state != MODEM_STATE_IDLE)) {
		g_debug("modem-libmodem: not in ringing or idle state");
		return -EBUSY;
	}

	g_debug("modem-libmodem: incoming call...");
	manager->state = MODEM_STATE_RINGING;

	events = remote_control_get_event_manager(manager->rc);
	if (!events)
		return -EINVAL;

	memset(&event, 0, sizeof(event));
	event.source = EVENT_SOURCE_MODEM;
	event.modem.state = EVENT_MODEM_STATE_RINGING;

	err = event_manager_report(events, &event);
	if (err < 0)
		g_debug("modem-libmodem: failed to report event: %s",
				strerror(-err));

	return err;
}

static int modem_dle(struct modem *modem, char dle, void *data)
{
	g_debug("modem-libmodem: DLE: %02x, '%c'", (unsigned char)dle,
			isprint(dle) ? dle : '?');
	return 0;
}

static const struct modem_callbacks callbacks = {
	.ring = modem_ring,
	.dle = modem_dle,
};

static gboolean modem_source_prepare(GSource *source, gint *timeout)
{
	if (timeout)
		*timeout = -1;

	return FALSE;
}

static gboolean modem_source_check(GSource *source)
{
	struct modem_manager *manager = (struct modem_manager *)source;

	if (manager->poll.revents & G_IO_IN)
		return TRUE;

	return FALSE;
}

static gboolean modem_source_dispatch(GSource *source, GSourceFunc callback,
		gpointer user_data)
{
	struct modem_manager *manager = (struct modem_manager *)source;
	gboolean ret = TRUE;
	char buffer[256];
	int err;

	err = modem_process(manager->modem, buffer, sizeof(buffer), 0);
	if (err < 0) {
		if (err != -ETIMEDOUT)
			g_debug("modem-libmodem: modem_process(): %s",
					strerror(-err));

		return TRUE;
	}

	if (callback)
		ret = callback(user_data);

	return ret;
}

static void modem_source_finalize(GSource *source)
{
	struct modem_manager *manager = (struct modem_manager *)source;

	modem_call_free(manager->call);
	modem_close(manager->modem);
	g_free(manager->number);
}

static GSourceFuncs modem_source_funcs = {
	.prepare = modem_source_prepare,
	.check = modem_source_check,
	.dispatch = modem_source_dispatch,
	.finalize = modem_source_finalize,
};

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

static int modem_manager_open_device(struct modem_manager *manager,
		const struct modem_desc *desc)
{
	struct modem_params params;
	int err;

	err = modem_open(&manager->modem, desc->device);
	if (err < 0)
		return err;

	memset(&params, 0, sizeof(params));

	err = modem_get_params(manager->modem, &params);
	if (err < 0) {
		modem_close(manager->modem);
		manager->modem = NULL;
		return err;
	}

	params.flags &= ~MODEM_FLAGS_ECHO;
	params.flags |= desc->flags;
	params.vls = desc->vls;
	params.atl = desc->atl;

	err = modem_set_params(manager->modem, &params);
	if (err < 0) {
		modem_close(manager->modem);
		manager->modem = NULL;
		return err;
	}

	g_debug("modem-libmodem: using device %s", desc->device);
	manager->flags = desc->flags;

	return err;
}

static int modem_manager_probe(struct modem_manager *manager)
{
	static const struct modem_desc modem_table[] = {
		{ "/dev/ttyACM0", MODEM_FLAGS_TOGGLE_HOOK, 1, 0 },
		{ "/dev/ttyS1", MODEM_FLAGS_DIRECT, 13, 3 },
		{ "/dev/ttyS0", MODEM_FLAGS_DIRECT, 13, 3 },
	};
	guint i;

	for (i = 0; i < G_N_ELEMENTS(modem_table); i++) {
		int err = modem_manager_open_device(manager, &modem_table[i]);
		if (err >= 0)
			break;

		g_debug("modem-libmodem: failed to use `%s' as modem",
				modem_table[i].device);
	}

	return (i < G_N_ELEMENTS(modem_table)) ? 0 : -ENODEV;
}

static int modem_manager_load_config(GKeyFile *config, struct modem_desc *desc)
{
	GError *error = NULL;
	const gchar *var;
	gchar *device;
	gulong flags;
	guint vls;
	guint atl;

	g_return_val_if_fail(config != NULL, -EINVAL);
	g_return_val_if_fail(desc != NULL, -EINVAL);

	if (!g_key_file_has_group(config, "modem")) {
		g_debug("modem-libmodem: no configuration for modem found");
		return -EIO;
	}

	device = g_key_file_get_value(config, "modem", "device", &error);
	if (error || !device) {
		var = "device";
		goto out;
	}

	flags = g_key_file_get_integer(config, "modem", "flags", &error);
	if (error) {
		var = "flags";
		goto out;
	}

	vls = g_key_file_get_integer(config, "modem", "vls", &error);
	if (error) {
		var = "vls";
		goto out;
	}

	atl = g_key_file_get_integer(config, "modem", "atl", &error);
	if (error) {
		var = "atl";
		goto out;
	}

	g_debug("modem-libmodem: configuration loaded: %s, flags:%lx, "
			"VLS:%u, ATL:%u", device, flags, vls, atl);

	desc->device = device;
	desc->flags = flags;
	desc->vls = vls;
	desc->atl = atl;

	return 0;

out:
	g_debug("modem-libmodem: %s: %s", var, error->message);
	g_clear_error(&error);
	g_free(device);
	return -EIO;
}

static int modem_manager_open(struct modem_manager *manager, GKeyFile *config)
{
	gboolean need_probe = TRUE;
	struct modem_desc desc;
	int ret;

	ret = modem_manager_load_config(config, &desc);
	if (ret >= 0) {
		ret = modem_manager_open_device(manager, &desc);
		if (ret < 0) {
			g_debug("modem-libmodem: failed to use `%s' as modem",
					desc.device);
		}

		g_free(desc.device);
		need_probe = FALSE;
	}

	if (need_probe) {
		g_debug("modem-libmodem: probing for device...");
		ret = modem_manager_probe(manager);
	}

	return ret;
}

int modem_manager_create(struct modem_manager **managerp,
		struct rpc_server *server, GKeyFile *config)
{
	struct modem_manager *manager;
	GSource *source;
	int err;

	g_return_val_if_fail(managerp != NULL, -EINVAL);
	g_return_val_if_fail(server != NULL, -EINVAL);

	modem_set_logv_func(modem_manager_logv);

	source = g_source_new(&modem_source_funcs, sizeof(*manager));
	if (!source)
		return -ENOMEM;

	manager = (struct modem_manager *)source;
	manager->rc = rpc_server_priv(server);
	manager->state = MODEM_STATE_IDLE;

	err = modem_manager_open(manager, config);
	if ((err < 0) && (err != -ENODEV))
		goto free;

	if (manager->modem) {
		err = modem_set_callbacks(manager->modem, &callbacks, manager);
		if (err < 0)
			goto free;

		manager->poll.events = G_IO_IN | G_IO_HUP | G_IO_ERR;
		manager->poll.fd = modem_get_fd(manager->modem);
		g_source_add_poll(source, &manager->poll);
	}

	*managerp = manager;
	return 0;

free:
	g_source_unref(source);
	return err;
}

GSource *modem_manager_get_source(struct modem_manager *manager)
{
	return manager ? &manager->source : NULL;
}

static int modem_manager_reset_modem(struct modem_manager *manager)
{
	int err;

	if (!manager->modem)
		return -ENODEV;

	g_free(manager->number);
	manager->number = NULL;

	err = modem_reset(manager->modem);
	if (err < 0) {
		g_debug("modem-libmodem: modem_reset(): %s", strerror(-err));
		return -err;
	}

	manager->state = MODEM_STATE_IDLE;
	return 0;
}

int modem_manager_initialize(struct modem_manager *manager)
{
	g_return_val_if_fail(manager != NULL, -EINVAL);

	return modem_manager_reset_modem(manager);
}

int modem_manager_shutdown(struct modem_manager *manager)
{
	g_return_val_if_fail(manager != NULL, -EINVAL);

	return modem_manager_reset_modem(manager);
}

int modem_manager_call(struct modem_manager *manager, const char *number)
{
	int err;

	g_return_val_if_fail(manager != NULL, -EINVAL);
	g_return_val_if_fail(number != NULL, -EINVAL);

	if (!manager->modem)
		return -ENODEV;

	if ((manager->state != MODEM_STATE_RINGING) &&
	    (manager->state != MODEM_STATE_IDLE)) {
		g_debug("modem-libmodem: not in ringing or idle state");
		return -EBUSY;
	}

	manager->state = MODEM_STATE_OUTGOING;

	g_free(manager->number);
	manager->number = g_strdup(number);

	err = modem_call(manager->modem, number);
	if (err < 0) {
		manager->state = MODEM_STATE_IDLE;
		return err;
	}

	manager->state = MODEM_STATE_ACTIVE;
	return 0;
}

int modem_manager_accept(struct modem_manager *manager)
{
	int err;

	g_return_val_if_fail(manager != NULL, -EINVAL);

	if (!manager->modem)
		return -ENODEV;

	if (manager->state != MODEM_STATE_RINGING) {
		if (manager->state == MODEM_STATE_IDLE) {
			g_debug("modem-libmodem: no incoming call");
			return -EBADF;
		}

		g_debug("modem-libmodem: call in progress");
		return -EBUSY;
	}

	manager->state = MODEM_STATE_INCOMING;

	err = modem_accept(manager->modem, NULL, 0);
	if (err < 0) {
		manager->state = MODEM_STATE_IDLE;
		return err;
	}

	manager->state = MODEM_STATE_ACTIVE;
	return 0;
}

int modem_manager_terminate(struct modem_manager *manager)
{
	int err = 0;

	g_return_val_if_fail(manager != NULL, -EINVAL);

	if (!manager->modem)
		return -ENODEV;

	if (manager->state == MODEM_STATE_IDLE) {
		g_debug("modem-libmodem: no call to terminate");
		return -EBADF;
	}

	if (manager->state == MODEM_STATE_RINGING) {
		g_debug("modem-libmodem: refusing incoming call");
		err = modem_reject(manager->modem);
	} else {
		g_debug("modem-libmodem: terminating call");
		err = modem_terminate(manager->modem);
	}

	/*
	 * Sometimes it happens that a call cannot be terminated properly,
	 * in which case there is nothing else we can do but to reset the
	 * modem.
	 */
	if (err < 0) {
		g_debug("modem-libmodem: recovering, resetting modem");
		err = modem_manager_reset_modem(manager);
	}

	manager->state = MODEM_STATE_IDLE;
	return err;
}

int modem_manager_get_state(struct modem_manager *manager, enum modem_state *statep)
{
	g_return_val_if_fail(manager != NULL, -EINVAL);
	g_return_val_if_fail(statep != NULL, -EINVAL);

	if (!manager->modem)
		return -ENODEV;

	*statep = manager->state;
	return 0;
}
