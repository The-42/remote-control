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

#if 0
int remote_voip_login(void *priv, struct remote_voip_account *account)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	struct medcom_voip_account login;
	int32_t ret = 0;
	int err;

	memset(&login, 0, sizeof(login));
	login.server = account->proxy;
	login.port = account->port;
	login.username = account->username;
	login.password = account->password;

	err = medcom_voip_login_stub(rpc, &ret, &login);
	if (err < 0)
		return err;

	return ret;
}
#else
struct medcom_voip_login_options {
	char *server;
	int auth;
	int transport;
	char *username;
	char *password;
	char server2[256];
	char username2[128];
	char password2[128];
};

remote_public
int remote_voip_login(void *priv, struct remote_voip_account *account)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	struct medcom_voip_login_options options;
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

	err = medcom_voip_login_stub(rpc, &ret, &buffer);
	if (err < 0) {
		ret = err;
		goto out;
	}

out:
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}
#endif

remote_public
int remote_voip_logout(void *priv)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int err;

	err = medcom_voip_logout_stub(rpc, &ret);
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

	err = medcom_voip_connect_to_stub(rpc, &ret, uri);
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

	err = medcom_voip_accept_incoming_stub(rpc, &ret, uri);
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

	err = medcom_voip_disconnect_stub(rpc, &ret);
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

	err = medcom_voip_still_logged_in_stub(rpc, &ret, status);
	if (err < 0)
		return err;

	return ret;
}
