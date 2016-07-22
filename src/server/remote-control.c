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

#include <netdb.h>
#include <glib.h>

#include "remote-control-stub.h"
#include "remote-control.h"

enum remote_control_state {
	REMOTE_CONTROL_UNCONNECTED,
	REMOTE_CONTROL_CONNECTED,
	REMOTE_CONTROL_IDLE,
	REMOTE_CONTROL_DISCONNECTED,
};

struct rpc_source {
	GSource source;

	struct remote_control *rc;
	enum remote_control_state state;
	char peer[NI_MAXHOST + 1];
	GPollFD poll_listen;
	GPollFD poll_client;
};

struct remote_control {
	struct event_manager *event_manager;
	struct gpio_backend *gpio;
	struct audio *audio;
	struct backlight *backlight;
	struct cursor_movement *cursor_movement;
	struct media_player *player;
	struct sound_manager *sound;
	struct smartcard *smartcard;
	struct voip *voip;
	struct mixer *mixer;
	struct net *net;
	struct lldp_monitor *lldp;
	struct task_manager *task_manager;
	struct handset *handset;
	struct app_watchdog *watchdog;

	GSource *source;
};

static int rpc_log(int priority, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = vprintf(fmt, ap);
	va_end(ap);

	return ret;
}

static gboolean rpc_source_prepare(GSource *source, gint *timeout)
{
	struct rpc_source *src = (struct rpc_source *)source;
	struct rpc_server *server = rpc_server_from_priv(src->rc);
	int err;

	switch (src->state) {
	case REMOTE_CONTROL_UNCONNECTED:
		break;

	case REMOTE_CONTROL_CONNECTED:
		err = rpc_server_get_client_socket(server);
		if (err < 0) {
			g_log(G_LOG_DOMAIN, G_LOG_LEVEL_ERROR,
					"rpc_server_get_client_socket(): %s",
					strerror(-err));
			src->state = REMOTE_CONTROL_UNCONNECTED;
			break;
		}

		src->poll_client.events = G_IO_IN | G_IO_HUP | G_IO_ERR;
		src->poll_client.fd = err;
		g_source_add_poll(source, &src->poll_client);

		src->state = REMOTE_CONTROL_IDLE;
		break;

	case REMOTE_CONTROL_IDLE:
		break;

	case REMOTE_CONTROL_DISCONNECTED:
		g_source_remove_poll(source, &src->poll_client);
		src->poll_client.events = 0;
		src->poll_client.fd = -1;
		src->state = REMOTE_CONTROL_UNCONNECTED;
		break;
	}

	if (timeout)
		*timeout = -1;

	return FALSE;
}

static gboolean rpc_source_check(GSource *source)
{
	struct rpc_source *src = (struct rpc_source *)source;

	/* handle server socket */
	if ((src->poll_listen.revents & G_IO_HUP) ||
	    (src->poll_listen.revents & G_IO_ERR))
		return FALSE;

	if (src->poll_listen.revents & G_IO_IN)
		return TRUE;

	/* handle client socket */
	if ((src->poll_client.revents & G_IO_HUP) ||
	    (src->poll_client.revents & G_IO_ERR)) {
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO, "connection closed by "
				"%s", src->peer);
		src->state = REMOTE_CONTROL_DISCONNECTED;
		return TRUE;
	}

	if (src->poll_client.revents & G_IO_IN)
		return TRUE;

	return FALSE;
}

static gboolean rpc_source_dispatch(GSource *source, GSourceFunc callback, gpointer user_data)
{
	struct rpc_source *src = (struct rpc_source *)source;
	struct rpc_server *server = rpc_server_from_priv(src->rc);
	struct rpc_packet *request = NULL;
	struct sockaddr *addr = NULL;
	gboolean ret = TRUE;
	int err;

	switch (src->state) {
	case REMOTE_CONTROL_UNCONNECTED:
		err = rpc_server_accept(server);
		if (err < 0) {
			g_log(G_LOG_DOMAIN, G_LOG_LEVEL_ERROR,
					"rpc_server_accept(): %s",
					strerror(-err));
			break;
		}

		err = rpc_server_get_peer(server, &addr);
		if ((err > 0) && addr) {
			err = getnameinfo(addr, err, src->peer, NI_MAXHOST,
					NULL, 0, NI_NUMERICHOST);
			if (!err) {
				g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
						"connection accepted from %s",
						src->peer);
			}

			free(addr);
		}

		src->state = REMOTE_CONTROL_CONNECTED;
		break;

	case REMOTE_CONTROL_CONNECTED:
		break;

	case REMOTE_CONTROL_IDLE:
		err = rpc_server_recv(server, &request);
		if ((err < 0) && (err != -ECONNRESET)) {
			g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
					"rpc_server_recv(): %s",
					strerror(-err));
			ret = FALSE;
			break;
		}

		if ((err == 0) || (err == -ECONNRESET)) {
			g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO, "connection "
					"closed by %s", src->peer);
			src->state = REMOTE_CONTROL_DISCONNECTED;
			break;
		}

		err = remote_control_dispatch(server, request);
		if (err < 0) {
			g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
					"rpc_dispatch(): %s", strerror(-err));
			rpc_packet_dump(request, rpc_log, 0);
			rpc_packet_free(request);
			ret = FALSE;
			break;
		}

		rpc_packet_free(request);
		break;

	case REMOTE_CONTROL_DISCONNECTED:
		break;
	}

	return ret;
}

static void rpc_source_finalize(GSource *source)
{
	struct rpc_source *src = (struct rpc_source *)source;
	remote_control_free(src->rc);
}

static GSourceFuncs rpc_source_funcs = {
	.prepare = rpc_source_prepare,
	.check = rpc_source_check,
	.dispatch = rpc_source_dispatch,
	.finalize = rpc_source_finalize,
};

static gboolean remote_control_source_prepare(GSource *source, gint *timeout)
{
	if (timeout)
		*timeout = -1;

	return FALSE;
}

static gboolean remote_control_source_check(GSource *source)
{
	return FALSE;
}

static gboolean remote_control_source_dispatch(GSource *source, GSourceFunc callback, gpointer user_data)
{
	gboolean ret = TRUE;

	if (callback)
		ret = callback(user_data);

	return ret;
}

static void remote_control_source_finalize(GSource *source)
{
}

static GSourceFuncs remote_control_source_funcs = {
	.prepare = remote_control_source_prepare,
	.check = remote_control_source_check,
	.dispatch = remote_control_source_dispatch,
	.finalize = remote_control_source_finalize,
};

static int socket_enable_keepalive(int socket, gboolean enable)
{
	int optval = enable ? 1 : 0;

	return setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, &optval,
			 sizeof(optval));
}

static gboolean config_get_socket_keepalive(GKeyFile *config)
{
	GError *error = NULL;
	gboolean enable;

	g_return_val_if_fail(config != NULL, false);

	enable = g_key_file_get_boolean(config, "general",
					"rpc-socket-keepalive", &error);
	if (error != NULL) {
		g_clear_error(&error);
		enable = false;
	}

	return enable;
}

int remote_control_create(struct remote_control **rcp, GKeyFile *config)
{
	struct rpc_server *server;
	struct remote_control *rc;
	struct rpc_source *src;
	GSource *source;
	int err, socket;

	if (!rcp)
		return -EINVAL;

	err = rpc_server_create(&server, NULL, sizeof(*rc));
	if (err < 0) {
		g_critical("rpc_server_create(): %s", strerror(-err));
		return err;
	}

	err = rpc_server_listen(server, 7482);
	if (err < 0) {
		g_critical("rpc_server_listen(): %s", strerror(-err));
		return err;
	}

	rc = rpc_server_priv(server);

	rc->source = g_source_new(&remote_control_source_funcs, sizeof(GSource));
	if (!rc->source) {
		g_critical("g_source_new() failed");
		return -ENOMEM;
	}

	err = lldp_monitor_create(&rc->lldp, config);
	if (err < 0) {
		g_critical("lldp_monitor_create(): %s", strerror(-err));
		return err;
	}

	source = lldp_monitor_get_source(rc->lldp);
	if (source) {
		g_source_add_child_source(rc->source, source);
		g_source_unref(source);
	}

	source = g_source_new(&rpc_source_funcs, sizeof(struct rpc_source));
	if (!source) {
		g_critical("g_source_new() failed");
		return -ENOMEM;
	}

	src = (struct rpc_source *)source;
	src->rc = rc;

	socket = rpc_server_get_listen_socket(server);
	if (socket < 0) {
		g_critical("rpc_server_get_listen(): %s", g_strerror(-socket));
		return socket;
	}

	err = socket_enable_keepalive(socket,
				      config_get_socket_keepalive(config));
	if (err < 0) {
		g_critical("socket_enable_keepalive(): %s", g_strerror(-err));
		return err;
	}

	src->poll_listen.events = G_IO_IN | G_IO_HUP | G_IO_ERR;
	src->poll_listen.fd = socket;

	g_source_add_poll(source, &src->poll_listen);
	g_source_add_child_source(rc->source, source);
	g_source_unref(source);

	err = event_manager_create(&rc->event_manager, server);
	if (err < 0) {
		g_critical("event_manager_create(): %s", strerror(-err));
		return err;
	}

	err = backlight_create(&rc->backlight);
	if (err < 0) {
		g_critical("backlight_create(): %s", strerror(-err));
		return err;
	}

	err = cursor_movement_create(&rc->cursor_movement);
	if (err < 0) {
		g_critical("cursor_movement_create(): %s", strerror(-err));
		return err;
	}

	err = media_player_create(&rc->player, config);
	if (err < 0) {
		g_critical("media_player_create(): %s", strerror(-err));
		return err;
	}

	err = audio_create(&rc->audio, server, config);
	if (err < 0) {
		g_critical("audio_create(): %s", strerror(-err));
		return err;
	}

	err = sound_manager_create(&rc->sound, rc->audio, config);
	if (err < 0) {
		g_critical("sound_manager_create(): %s", strerror(-err));
		return err;
	}

	err = smartcard_create(&rc->smartcard, server, config);
	if (err < 0) {
		g_critical("smartcard_create(): %s", strerror(-err));
		return err;
	}

	err = voip_create(&rc->voip, server, config);
	if (err < 0) {
		g_critical("voip_create(): %s", strerror(-err));
		return err;
	}

	source = voip_get_source(rc->voip);
	if (source) {
		g_source_add_child_source(rc->source, source);
		g_source_unref(source);
	}

	err = net_create(&rc->net, server);
	if (err < 0) {
		g_critical("net_create(): %s", strerror(-err));
		return err;
	}

	err = task_manager_create(&rc->task_manager);
	if (err < 0) {
		g_critical("task_manager_create(): %s", strerror(-err));
		return err;
	}

	err = handset_create(&rc->handset, server);
	if (err < 0) {
		g_critical("handset_create(): %s", strerror(-err));
		return err;
	}

	err = mixer_create(&rc->mixer);
	if (err < 0) {
		g_critical("mixer_create(): %s", strerror(-err));
		return err;
	}

	source = mixer_get_source(rc->mixer);
	if (source) {
		g_source_add_child_source(rc->source, source);
		g_source_unref(source);
	}

	err = gpio_backend_create(&rc->gpio, rc->event_manager, config);
	if (err < 0) {
		g_critical("gpio_backend_create(): %s", strerror(-err));
		return err;
	}

	source = gpio_backend_get_source(rc->gpio);
	if (source) {
		g_source_add_child_source(rc->source, source);
		g_source_unref(source);
	}

	err = usb_handset_create(rc);
	if (err < 0) {
		g_critical("usb_handset_create(): %s", strerror(-err));
		return err;
	}

	err = app_watchdog_create(&rc->watchdog, config);
	if (err < 0) {
		g_critical("app_watchdog_create(): %s", strerror(-err));
		return err;
	}

	*rcp = rc;
	return 0;
}

GSource *remote_control_get_source(struct remote_control *rc)
{
	return rc->source;
}

int remote_control_free(struct remote_control *rc)
{
	struct rpc_server *server = rpc_server_from_priv(rc);

	if (!rc)
		return -EINVAL;

	handset_free(rc->handset);
	task_manager_free(rc->task_manager);
	net_free(rc->net);
	voip_free(rc->voip);
	smartcard_free(rc->smartcard);
	sound_manager_free(rc->sound);
	media_player_free(rc->player);
	cursor_movement_free(rc->cursor_movement);
	backlight_free(rc->backlight);
	audio_free(rc->audio);
	app_watchdog_free(rc->watchdog);
	gpio_backend_free(rc->gpio);
	event_manager_free(rc->event_manager);
	lldp_monitor_free(rc->lldp);
	rpc_server_free(server);

	return 0;
}

struct event_manager *remote_control_get_event_manager(struct remote_control *rc)
{
	return rc ? rc->event_manager : NULL;
}

struct audio *remote_control_get_audio(struct remote_control *rc)
{
	return rc ? rc->audio : NULL;
}

struct backlight *remote_control_get_backlight(struct remote_control *rc)
{
	return rc ? rc->backlight : NULL;
}

struct cursor_movement *remote_control_get_cursor_movement(struct remote_control *rc)
{
	return rc ? rc->cursor_movement : NULL;
}

struct media_player *remote_control_get_media_player(struct remote_control *rc)
{
	return rc ? rc->player : NULL;
}

struct sound_manager *remote_control_get_sound_manager(struct remote_control *rc)
{
	return rc ? rc->sound : NULL;
}

struct smartcard *remote_control_get_smartcard(struct remote_control *rc)
{
	return rc ? rc->smartcard : NULL;
}

struct voip *remote_control_get_voip(struct remote_control *rc)
{
	return rc ? rc->voip : NULL;
}

struct mixer *remote_control_get_mixer(struct remote_control *rc)
{
	return rc ? rc->mixer : NULL;
}

struct net *remote_control_get_net(struct remote_control *rc)
{
	return rc ? rc->net : NULL;
}

struct lldp_monitor *remote_control_get_lldp_monitor(struct remote_control *rc)
{
	return rc ? rc->lldp : NULL;
}

struct task_manager *remote_control_get_task_manager(struct remote_control *rc)
{
	return rc ? rc->task_manager : NULL;
}

struct handset *remote_control_get_handset(struct remote_control *rc)
{
	return rc ? rc->handset : NULL;
}

struct gpio_backend *remote_control_get_gpio_backend(struct remote_control *rc)
{
	return rc ? rc->gpio : NULL;
}

struct app_watchdog *remote_control_get_watchdog(struct remote_control *rc)
{
	return rc ? rc->watchdog : NULL;
}

int remote_control_dispatch(struct rpc_server *server, struct rpc_packet *request)
{
	return rpc_dispatch(server, request);
}
