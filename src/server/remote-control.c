/*
 * Copyright (C) 2010-2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <glib.h>

#include "remote-control-stub.h"
#include "remote-control.h"

struct remote_control {
	struct event_manager *event_manager;
	struct backlight *backlight;
	struct media_player *player;
	struct smartcard *smartcard;
	struct voip *voip;
	struct mixer *mixer;
	struct net *net;
	struct lldp_monitor *lldp;
};

int remote_control_create(struct remote_control **rcp)
{
	struct rpc_server *server;
	struct remote_control *rc;
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

	err = smartcard_create(&rc->smartcard);
	if (err < 0) {
		g_error("smartcard_create(): %s", strerror(-err));
		return err;
	}

	err = voip_create(&rc->voip, server);
	if (err < 0) {
		g_error("voip_create(): %s", strerror(-err));
		return err;
	}

	err = net_create(&rc->net);
	if (err < 0) {
		g_error("net_create(): %s", strerror(-err));
		return err;
	}

	err = lldp_monitor_create(&rc->lldp);
	if (err < 0) {
		g_error("lldp_monitor_create(): %s", strerror(-err));
		return err;
	}

	err = mixer_create(&rc->mixer);
	if (err < 0) {
		g_error("mixer_create(): %s", strerror(-err));
		return err;
	}

	*rcp = rc;
	return 0;
}

int remote_control_free(struct remote_control *rc)
{
	struct rpc_server *server = rpc_server_from_priv(rc);

	if (!rc)
		return -EINVAL;

	mixer_free(rc->mixer);
	lldp_monitor_free(rc->lldp);
	net_free(rc->net);
	voip_free(rc->voip);
	smartcard_free(rc->smartcard);
	media_player_free(rc->player);
	backlight_free(rc->backlight);
	event_manager_free(rc->event_manager);
	rpc_server_free(server);
	return 0;
}

struct event_manager *remote_control_get_event_manager(struct remote_control *rc)
{
	return rc ? rc->event_manager : NULL;
}

struct backlight *remote_control_get_backlight(struct remote_control *rc)
{
	return rc ? rc->backlight : NULL;
}

struct media_player *remote_control_get_media_player(struct remote_control *rc)
{
	return rc ? rc->player : NULL;
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

int remote_control_dispatch(struct rpc_server *server, struct rpc_packet *request)
{
	return rpc_dispatch(server, request);
}
