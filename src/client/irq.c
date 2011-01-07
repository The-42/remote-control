/*
 * Copyright (C) 2010-2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <glib.h>
#include "remote-control.h"

static void notify_event_handlers(enum medcom_irq_source source, uint32_t status)
{
	fprintf(stdout, "%s: irq %d changed to 0x%08x\n", __func__, source, status);
}

struct irq_status {
	enum medcom_irq_source source;
	uint32_t status;
};

static int poll_irq_status(void *priv)
{
	static struct irq_status table[] = {
		{ MEDCOM_IRQ_SOURCE_HOOK, 0 },
		{ MEDCOM_IRQ_SOURCE_CARD, 0 },
		{ MEDCOM_IRQ_SOURCE_VOIP, 0 },
		{ MEDCOM_IRQ_SOURCE_IO,   0 },
		{ MEDCOM_IRQ_SOURCE_RDP,  0 }
	};
	uint32_t mask = 0;
	uint32_t old;
	int ret;
	int i;

//	ret = medcom_irq_enable(client, TRUE);

	ret = medcom_irq_get_mask(priv, &mask);
	if (ret < 0) {
		fprintf(stderr, "unable to get irq mask [%s]\n", strerror(-ret));
		return ret;
	}

	for (i = 0; i < G_N_ELEMENTS(table); i++) {
		if (mask & table[i].source) {
			old = table[i].status;
			ret = medcom_irq_get_info(priv, table[i].source, &table[i].status);
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
void medcom_irq_event(void *priv, uint32_t type)
{
//	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int ret;

	printf("> %s(priv=%p, type=%x)\n", __func__, priv, type);

	ret = poll_irq_status(priv);

	printf("< %s()\n", __func__);
}

int32_t medcom_irq_enable(void *priv, uint8_t virtkey)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int err;

	err = medcom_irq_enable_stub(rpc, &ret, virtkey);
	if (err < 0)
		return err;

	return ret;
}

int32_t medcom_irq_get_mask(void *priv, uint32_t *mask)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int err;

	if (!mask)
		return -EINVAL;

	err = medcom_irq_get_mask_stub(rpc, &ret, mask);
	if (err < 0)
		return err;

	return ret;
}

int32_t medcom_irq_get_info(void *priv, enum medcom_irq_source source, uint32_t *info)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int err;

	if (!info)
		return -EINVAL;

	err = medcom_irq_get_info_stub(rpc, &ret, source, info);
	if (err < 0)
		return err;

	return ret;
}
