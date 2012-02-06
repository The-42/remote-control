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
	struct gpio_chip *gpio;
	struct audio *audio;
	struct backlight *backlight;
	struct media_player *player;
	struct sound_manager *sound;
	struct smartcard *smartcard;
	struct rfid *rfid;
	struct modem_manager *modem;
	struct voip *voip;
	struct mixer *mixer;
	struct net *net;
	struct lldp_monitor *lldp;
	struct task_manager *task_manager;
	struct tuner *tuner;
	struct handset *handset;

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

int remote_control_create(struct remote_control **rcp, GKeyFile *config)
{
	struct rpc_server *server;
	struct remote_control *rc;
	struct rpc_source *src;
	GSource *source;
	int err;

	if (!rcp)
		return -EINVAL;

	err = rpc_server_create(&server, NULL, sizeof(*rc));
	if (err < 0) {
		g_error("rpc_server_create(): %s", strerror(-err));
		return err;
	}

	err = rpc_server_listen(server, 7482);
	if (err < 0) {
		g_error("rpc_server_listen(): %s", strerror(-err));
		return err;
	}

	rc = rpc_server_priv(server);

	rc->source = g_source_new(&remote_control_source_funcs, sizeof(GSource));
	if (!rc->source) {
		g_error("g_source_new() failed");
		return -ENOMEM;
	}

	source = g_source_new(&rpc_source_funcs, sizeof(struct rpc_source));
	if (!source) {
		g_error("g_source_new() failed");
		return -ENOMEM;
	}

	src = (struct rpc_source *)source;
	src->rc = rc;

	err = rpc_server_get_listen_socket(server);
	if (err < 0) {
		g_error("rpc_server_get_listen(): %s", strerror(-err));
		return err;
	}

	src->poll_listen.events = G_IO_IN | G_IO_HUP | G_IO_ERR;
	src->poll_listen.fd = err;

	g_source_add_poll(source, &src->poll_listen);
	g_source_add_child_source(rc->source, source);
	g_source_unref(source);

	err = event_manager_create(&rc->event_manager, server);
	if (err < 0) {
		g_error("event_manager_create(): %s", strerror(-err));
		return err;
	}

	err = backlight_create(&rc->backlight);
	if (err < 0) {
		g_error("backlight_create(): %s", strerror(-err));
		return err;
	}

	err = media_player_create(&rc->player);
	if (err < 0) {
		g_error("media_player_create(): %s", strerror(-err));
		return err;
	}

	err = sound_manager_create(&rc->sound);
	if (err < 0) {
		g_error("sound_manager_create(): %s", strerror(-err));
		return err;
	}

	err = audio_create(&rc->audio, server);
	if (err < 0) {
		g_error("audio_create(): %s", strerror(-err));
		return err;
	}

	err = smartcard_create(&rc->smartcard, server);
	if (err < 0) {
		g_error("smartcard_create(): %s", strerror(-err));
		return err;
	}

	err = rfid_create(&rc->rfid, server);
	if (err < 0) {
		g_error("rfid_create(): %s", strerror(-err));
		return err;
	}

	err = modem_manager_create(&rc->modem, server, config);
	if (err < 0) {
		g_error("modem_manager_create(): %s", strerror(-err));
		return err;
	}

	source = modem_manager_get_source(rc->modem);
	if (source) {
		g_source_add_child_source(rc->source, source);
		g_source_unref(source);
	}

	err = voip_create(&rc->voip, server);
	if (err < 0) {
		g_error("voip_create(): %s", strerror(-err));
		return err;
	}

	source = voip_get_source(rc->voip);
	if (source) {
		g_source_add_child_source(rc->source, source);
		g_source_unref(source);
	}

	err = net_create(&rc->net, server);
	if (err < 0) {
		g_error("net_create(): %s", strerror(-err));
		return err;
	}

	err = lldp_monitor_create(&rc->lldp);
	if (err < 0) {
		g_error("lldp_monitor_create(): %s", strerror(-err));
		return err;
	}

	source = lldp_monitor_get_source(rc->lldp);
	if (source) {
		g_source_add_child_source(rc->source, source);
		g_source_unref(source);
	}

	err = task_manager_create(&rc->task_manager);
	if (err < 0) {
		g_error("task_manager_create(): %s", strerror(-err));
		return err;
	}

	err = tuner_create(&rc->tuner);
	if (err < 0) {
		g_error("tuner_create(): %s", strerror(-err));
		return err;
	}

	err = handset_create(&rc->handset, server);
	if (err < 0) {
		g_error("handset_create(): %s", strerror(-err));
		return err;
	}

	err = mixer_create(&rc->mixer);
	if (err < 0) {
		g_error("mixer_create(): %s", strerror(-err));
		return err;
	}

	source = mixer_get_source(rc->mixer);
	if (source) {
		g_source_add_child_source(rc->source, source);
		g_source_unref(source);
	}

	err = gpio_chip_create(&rc->gpio, rc->event_manager);
	if (err < 0) {
		g_error("gpio_chip_create(): %s", strerror(-err));
		return err;
	}

	source = gpio_chip_get_source(rc->gpio);
	if (source) {
		g_source_add_child_source(rc->source, source);
		g_source_unref(source);
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
	tuner_free(rc->tuner);
	task_manager_free(rc->task_manager);
	net_free(rc->net);
	voip_free(rc->voip);
	rfid_free(rc->rfid);
	smartcard_free(rc->smartcard);
	sound_manager_free(rc->sound);
	media_player_free(rc->player);
	backlight_free(rc->backlight);
	audio_free(rc->audio);
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

struct rfid *remote_control_get_rfid(struct remote_control *rc)
{
	return rc ? rc->rfid : NULL;
}

struct modem_manager *remote_control_get_modem_manager(struct remote_control *rc)
{
	return rc ? rc->modem : NULL;
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

struct tuner *remote_control_get_tuner(struct remote_control *rc)
{
	return rc ? rc->tuner : NULL;
}

struct handset *remote_control_get_handset(struct remote_control *rc)
{
	return rc ? rc->handset : NULL;
}

struct gpio_chip *remote_control_get_gpio_chip(struct remote_control *rc)
{
	return rc ? rc->gpio : NULL;
}

int remote_control_dispatch(struct rpc_server *server, struct rpc_packet *request)
{
	return rpc_dispatch(server, request);
}
