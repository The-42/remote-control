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

static void notify_event_handlers(enum remote_irq_source source,
		uint32_t status)
{
	fprintf(stdout, "%s: irq %d changed to 0x%08x\n", __func__, source,
			status);
}

struct irq_status {
	enum remote_irq_source source;
	uint32_t status;
};

static int poll_irq_status(void *priv)
{
	static struct irq_status table[] = {
		{ REMOTE_IRQ_SOURCE_HANDSET,   0 },
		{ REMOTE_IRQ_SOURCE_SMARTCARD, 0 },
		{ REMOTE_IRQ_SOURCE_VOIP,      0 },
		{ REMOTE_IRQ_SOURCE_IO,        0 },
		{ REMOTE_IRQ_SOURCE_RDP,       0 }
	};
	uint32_t mask = 0;
	uint32_t old;
	int ret;
	int i;

//	ret = remote_irq_enable(client, TRUE);

	ret = remote_irq_get_mask(priv, &mask);
	if (ret < 0) {
		fprintf(stderr, "unable to get irq mask [%s]\n", strerror(-ret));
		return ret;
	}

	for (i = 0; i < G_N_ELEMENTS(table); i++) {
		if (mask & table[i].source) {
			old = table[i].status;
			ret = remote_irq_get_info(priv, table[i].source, &table[i].status);
			if (ret < 0) {
				fprintf(stderr, "unable to get irq %d info [%s]\n",
					table[i].source, strerror(-ret));
				continue;
			}
			fprintf(stdout, "IRQ: %d changed 0x%08x -> 0x%08x\n",
				table[i].source, old, table[i].status);
			if (old != table[i].status)
				notify_event_handlers(table[i].source, table[i].status);
		}
	}

	return 0;
}

/**
 * This is the ugly way....
 */
void RPC_IMPL(irq_event)(void *priv, uint32_t type)
{
	printf("> %s(priv=%p, type=%x)\n", __func__, priv, type);

	poll_irq_status(priv);

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
