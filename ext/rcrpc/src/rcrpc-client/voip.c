/*
 * Copyright (C) 2010-2011 Avionic Design GmbH
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
#include "remote-control.h"

struct RPC_TYPE(voip_login_options) {
	uint32_t server_ptr;
	uint32_t auth;
	uint32_t transport;
	uint32_t username_ptr;
	uint32_t password_ptr;
	char server2[256];
	char username2[128];
	char password2[128];
};

remote_public
int remote_voip_login(void *priv, struct remote_voip_account *account)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	struct RPC_TYPE(voip_login_options) options;
	struct rpc_buffer buffer;
	int32_t ret = 0;
	int err = 0;

	g_debug("> %s(priv=%p, account=%p)", __func__, priv, (void *)account);

	memset(&options, 0, sizeof(options));
	strncpy(options.server2, account->proxy, sizeof(options.server2));
	strncpy(options.username2, account->username, sizeof(options.username2));
	strncpy(options.password2, account->password, sizeof(options.password2));

	memset(&buffer, 0, sizeof(buffer));
	buffer.tx_buf = &options;
	buffer.tx_num = sizeof(options);

	err = RPC_STUB(voip_login)(rpc, &ret, &buffer);
	if (err < 0) {
		ret = err;
		goto out;
	}

out:
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

remote_public
int remote_voip_logout(void *priv)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int err;

	err = RPC_STUB(voip_logout)(rpc, &ret);
	if (err < 0)
		return err;

	return ret;
}

remote_public
int remote_voip_connect_to(void *priv, const char *uri)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int err;

	err = RPC_STUB(voip_connect_to)(rpc, &ret, uri);
	if (err < 0)
		return err;

	return ret;
}

remote_public
int remote_voip_accept_incoming(void *priv, char **uri)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int err;

	err = RPC_STUB(voip_accept_incoming)(rpc, &ret, uri);
	if (err < 0)
		return err;

	return ret;
}

remote_public
int remote_voip_disconnect(void *priv)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int err;

	err = RPC_STUB(voip_disconnect)(rpc, &ret);
	if (err < 0)
		return err;

	return ret;
}

remote_public
int remote_voip_still_logged_in(void *priv, bool *status)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int err;

	err = RPC_STUB(voip_still_logged_in)(rpc, &ret, status);
	if (err < 0)
		return err;

	return ret;
}

remote_public
int remote_voip_dial(void *priv, uint8_t dtmf)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int err;

	err = RPC_STUB(voip_dial)(rpc, &ret, dtmf);
	if (err < 0)
		return err;

	return ret;
}
