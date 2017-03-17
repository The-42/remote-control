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

#include <glib.h>

#include "remote-control-stub.h"
#include "rcrpc-client.h"

remote_public
int remote_modem_init(void *priv)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int err = 0;

	err = RPC_STUB(modem_init)(rpc, &ret);
	if (err < 0) {
		ret = err;
		goto out;
	}

out:
	return ret;
}

remote_public
int remote_modem_deinit(void *priv)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int err = 0;

	err = RPC_STUB(modem_deinit)(rpc, &ret);
	if (err < 0) {
		ret = err;
		goto out;
	}

out:
	return ret;
}

remote_public
int remote_modem_pick_up(void *priv)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int err = 0;

	err = RPC_STUB(modem_pick_up)(rpc, &ret);
	if (err < 0) {
		ret = err;
		goto out;
	}

out:
	return ret;
}

remote_public
int remote_modem_hang_up(void *priv)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int err = 0;

	err = RPC_STUB(modem_hang_up)(rpc, &ret);
	if (err < 0) {
		ret = err;
		goto out;
	}

out:
	return ret;
}

remote_public
int remote_modem_dial(void *priv, const char *number)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int err = 0;

	err = RPC_STUB(modem_dial)(rpc, &ret, number);
	if (err < 0) {
		ret = err;
		goto out;
	}

out:
	return ret;
}

remote_public
int remote_modem_is_calling(void *priv, bool *status)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int err = 0;

	err = RPC_STUB(modem_is_calling)(rpc, &ret, status);
	if (err < 0) {
		ret = err;
		goto out;
	}

out:
	return ret;
}

remote_public
int remote_modem_is_connected(void *priv, bool *status)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int err = 0;

	err = RPC_STUB(modem_is_connected)(rpc, &ret, status);
	if (err < 0) {
		ret = err;
		goto out;
	}

out:
	return ret;
}
