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

int32_t RPC_IMPL(irq_enable)(void *priv, uint8_t virtkey)
{
	struct rpc_server *server = rpc_server_from_priv(priv);
	int err;

	err = RPC_STUB(irq_event)(server, 0);
	if (err < 0) {
		g_debug("irq_event(): %s", strerror(-err));
		return err;
	}

	return 0;
}

int32_t RPC_IMPL(irq_get_mask)(void *priv, uint32_t *mask)
{
	int32_t ret = 0;

	g_debug("> %s(priv=%p, mask=%p)", __func__, priv, mask);

	if (!priv || !mask)
		ret = -EINVAL;

	*mask = 0;

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(irq_get_info)(void *priv, enum RPC_TYPE(irq_source) source, uint32_t *info)
{
	int32_t ret = -EINVAL;

	g_debug("> %s(priv=%p, source=%d, info=%p)", __func__, priv, source, info);

	return ret;
}
