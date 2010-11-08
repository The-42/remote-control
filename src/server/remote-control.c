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

	err = rpc_server_create(&server, sizeof(*rc));
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

	err = media_player_create(&rc->player);
	if (err < 0) {
		g_error("media_player_create(): %s", strerror(-err));
		return err;
	}

	err = voip_create(&rc->voip, server);
	if (err < 0) {
		g_error("voip_create(): %s", strerror(-err));
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

	voip_free(rc->voip);
	media_player_free(rc->player);
	rpc_server_free(server);
	return 0;
}

int remote_control_dispatch(struct rpc_server *server, struct rpc_packet *request)
{
	return rpc_dispatch(server, request);
}
