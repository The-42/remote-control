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

int32_t RPC_IMPL(check_io)(void *priv, uint8_t *value)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, value=%p)", __func__, priv, value);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(justaboard_set_mask)(void *priv, uint32_t mask)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, mask=%#x)", __func__, priv, mask);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(justaboard_get_mask)(void *priv, uint32_t *mask)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, mask=%p)", __func__, priv, mask);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}
