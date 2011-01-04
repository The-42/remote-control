#include <glib.h>

#include "remote-control-stub.h"
#include "remote-control.h"

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

int remote_control_dispatch(struct rpc_server *server, struct rpc_packet *request)
{
	return rpc_dispatch(server, request);
}
