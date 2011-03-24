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

int32_t RPC_IMPL(modem_init)(void *priv)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p)", __func__, priv);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(modem_deinit)(void *priv)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p)", __func__, priv);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(modem_pick_up)(void *priv)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p)", __func__, priv);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(modem_hang_up)(void *priv)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p)", __func__, priv);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(modem_dial)(void *priv, const char *number)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, number=%s)", __func__, priv, number);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(modem_is_calling)(void *priv, bool *calling)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, calling=%p)", __func__, priv, calling);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(modem_is_connected)(void *priv, bool *connected)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, connected=%p)", __func__, priv, connected);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}
