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
#include "remote-control-rpc.h"
#include "remote-control.h"

int32_t RPC_IMPL(modem_init)(void *priv)
{
	struct modem_manager *modem = remote_control_get_modem_manager(RCPTR(priv));
	int32_t ret;

	g_debug("> %s(priv=%p)", __func__, priv);

	ret = modem_manager_initialize(modem);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(modem_deinit)(void *priv)
{
	struct modem_manager *modem = remote_control_get_modem_manager(RCPTR(priv));
	int32_t ret;

	g_debug("> %s(priv=%p)", __func__, priv);

	ret = modem_manager_shutdown(modem);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(modem_pick_up)(void *priv)
{
	struct modem_manager *modem = remote_control_get_modem_manager(RCPTR(priv));
	int32_t ret;

	g_debug("> %s(priv=%p)", __func__, priv);

	ret = modem_manager_accept(modem);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(modem_hang_up)(void *priv)
{
	struct modem_manager *modem = remote_control_get_modem_manager(RCPTR(priv));
	int32_t ret;

	g_debug("> %s(priv=%p)", __func__, priv);

	ret = modem_manager_terminate(modem);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(modem_dial)(void *priv, const char *number)
{
	struct modem_manager *modem = remote_control_get_modem_manager(RCPTR(priv));
	int32_t ret;

	g_debug("> %s(priv=%p, number=%s)", __func__, priv, number);

	ret = modem_manager_call(modem, number);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(modem_is_calling)(void *priv, bool *calling)
{
	struct modem_manager *modem = remote_control_get_modem_manager(RCPTR(priv));
	enum modem_state state = MODEM_STATE_IDLE;
	int32_t ret;

	g_debug("> %s(priv=%p, calling=%p)", __func__, priv, calling);

	ret = modem_manager_get_state(modem, &state);
	if (ret < 0)
		goto out;

	switch (state) {
	case MODEM_STATE_IDLE:
	default:
		*calling = false;
		break;
	}

out:
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(modem_is_connected)(void *priv, bool *connected)
{
	struct modem_manager *modem = remote_control_get_modem_manager(RCPTR(priv));
	enum modem_state state = MODEM_STATE_IDLE;
	int32_t ret;

	g_debug("> %s(priv=%p, connected=%p)", __func__, priv, connected);

	ret = modem_manager_get_state(modem, &state);
	if (ret < 0)
		goto out;

	switch (state) {
	case MODEM_STATE_IDLE:
	default:
		*connected = false;
		break;
	}

out:
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}
