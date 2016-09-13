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

int32_t RPC_IMPL(card_get_type)(void *priv, enum RPC_TYPE(card_type) *typep)
{
	int32_t ret = 0;

	g_debug("> %s(priv=%p, type=%p)", __func__, priv, typep);

	*typep = RPC_MACRO(CARD_TYPE_UNKNOWN);

	return ret;
}

int32_t RPC_IMPL(card_read)(void *priv, off_t offset, struct rpc_buffer *buffer)
{
	int32_t ret = 0;

	g_debug("> %s(priv=%p, offset=%ld, buffer=%p)", __func__, priv, offset,
			buffer);

	return ret;
}

int32_t RPC_IMPL(card_write)(void *priv, off_t offset, struct rpc_buffer *buffer)
{
	int32_t ret = 0;

	g_debug("> %s(priv=%p, offset=%ld, buffer=%p)", __func__, priv, offset,
			buffer);

	return ret;
}
