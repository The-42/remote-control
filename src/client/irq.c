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

void RPC_IMPL(irq_event)(void *priv, uint32_t type)
{
	printf("> %s(priv=%p, type=%x)\n", __func__, priv, type);
	printf("< %s()\n", __func__);
}

int remote_irq_enable(void *priv, uint8_t virtkey)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int err;

	err = RPC_STUB(irq_enable)(rpc, &ret, virtkey);
	if (err < 0)
		return err;

	return ret;
}

int remote_irq_get_mask(void *priv, uint32_t *mask)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int err;

	if (!mask)
		return -EINVAL;

	err = RPC_STUB(irq_get_mask)(rpc, &ret, mask);
	if (err < 0)
		return err;

	return ret;
}

int remote_irq_get_info(void *priv, enum remote_irq_source source, uint32_t *info)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int err;

	if (!info)
		return -EINVAL;

	err = RPC_STUB(irq_get_info)(rpc, &ret, source, info);
	if (err < 0)
		return err;

	return ret;
}
