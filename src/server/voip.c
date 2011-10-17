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

int32_t RPC_IMPL(voip_get_version)(void *priv, uint32_t *version)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, version=%p)", __func__, priv, version);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(voip_get_status)(void *priv, uint32_t *status)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, status=%p)", __func__, priv, status);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(voip_get_status_string)(void *priv, uint32_t status, char **buffer)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, status=%#x, buffer=%p)", __func__, priv, status,
			buffer);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

#if 0
int32_t RPC_IMPL(voip_login)(void *priv, struct RPC_TYPE(voip_account) *account)
{
	struct remote_control *rc = priv;
	int32_t ret;

	g_debug("> %s(priv=%p, account=%p)", __func__, priv, account);
	g_debug("  account:");
	g_debug("    server: %s", account->server);
	g_debug("    port: %u", account->port);
	g_debug("    username: %s", account->username);
	g_debug("    password: %s", account->password);

	ret = voip_login(rc->voip, account->server, account->port,
			account->username, account->password);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}
#else
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

int32_t RPC_IMPL(voip_login)(void *priv, struct rpc_buffer *options)
{
	struct voip *voip = remote_control_get_voip(priv);
	struct RPC_TYPE(voip_login_options) *account;
	int32_t ret;

	g_debug("> %s(priv=%p, options=%p)", __func__, priv, options);
	account = options->rx_buf;

	ret = voip_login(voip, account->server2, 5060, account->username2,
			account->password2);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}
#endif

int32_t RPC_IMPL(voip_logout)(void *priv)
{
	struct voip *voip = remote_control_get_voip(priv);
	int32_t ret;

	g_debug("> %s(priv=%p)", __func__, priv);

	ret = voip_logout(voip);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(voip_still_logged_in)(void *priv, bool *status)
{
	struct voip *voip = remote_control_get_voip(priv);
	enum voip_state state = VOIP_STATE_LOGGED_OUT;
	int32_t ret = 0;

	g_debug("> %s(priv=%p, status=%p)", __func__, priv, status);

	ret = voip_get_state(voip, &state);
	if (ret < 0)
		goto out;

	if (state == VOIP_STATE_LOGGED_IN)
		*status = true;
	else
		*status = false;

out:
	g_debug("< %s(status=%s) = %d", __func__, *status ? "true" : "false", ret);
	return ret;
}

int32_t RPC_IMPL(voip_connect_to)(void *priv, const char *uri)
{
	struct voip *voip = remote_control_get_voip(priv);
	int32_t ret;

	g_debug("> %s(priv=%p, uri=%p)", __func__, priv, uri);

	ret = voip_call(voip, uri);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(voip_accept_incoming)(void *priv, char **uri)
{
	struct voip *voip = remote_control_get_voip(priv);
	int32_t ret;

	g_debug("> %s(priv=%p, uri=%p)", __func__, priv, uri);

	ret = voip_accept(voip, uri);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(voip_disconnect)(void *priv)
{
	struct voip *voip = remote_control_get_voip(priv);
	int32_t ret;

	g_debug("> %s(priv=%p)", __func__, priv);

	ret = voip_terminate(voip);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(voip_is_connected)(void *priv, bool *state)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, state=%p)", __func__, priv, state);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(voip_is_calling)(void *priv, bool *state)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, state=%p)", __func__, priv, state);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(voip_get_last_contact)(void *priv, char **contact)
{
	struct voip *voip = remote_control_get_voip(priv);
	const char *name = NULL;
	int32_t ret;

	g_debug("> %s(priv=%p, contact=%p)", __func__, priv, contact);

	if (!priv || !contact) {
		ret = -EINVAL;
		goto out;
	}

	ret = voip_get_contact(voip, &name);
	if (ret < 0)
		goto out;

	if (name) {
		char *copy = strdup(name);
		if (copy)
			*contact = copy;
		else
			ret = -ENOMEM;
	}

out:
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(voip_dial)(void *priv, int8_t dtmf)
{
	struct voip *voip = remote_control_get_voip(priv);
	int32_t ret;

	g_debug("> %s(priv=%p, dtmf=%c)", __func__, priv, dtmf);

	if (!priv) {
		ret = -EINVAL;
		goto out;
	}

	ret = voip_dial(voip, dtmf);
	if (ret < 0)
		goto out;

out:
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}
