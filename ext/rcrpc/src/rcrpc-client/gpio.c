/*
 * Copyright (C) 2012 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "remote-control-stub.h"
#include "rcrpc-client.h"

#define BIT(x) (1 << (x))

remote_public
int remote_gpio_set(void *priv, unsigned int gpio, int value)
{
	struct rpc_client *client = rpc_client_from_priv(priv);
	uint32_t mask = 0;
	int32_t ret = 0;
	int err;

	err = RPC_STUB(gpio_get_mask)(client, &ret, &mask);
	if (err < 0)
		return err;
	else if (ret < 0)
		return ret;

	if (value)
		mask |= BIT(gpio);
	else
		mask &= ~BIT(gpio);

	err = RPC_STUB(gpio_set_mask)(client, &ret, mask);
	if (err < 0)
		return err;

	return ret;
}

remote_public
int remote_gpio_get(void *priv, unsigned int gpio)
{
	struct rpc_client *client = rpc_client_from_priv(priv);
	uint32_t mask = 0;
	int32_t ret = 0;
	int err;

	err = RPC_STUB(gpio_get_mask)(client, &ret, &mask);
	if (err < 0)
		return err;
	else if (ret < 0)
		return ret;

	return (mask >> gpio) & 1;
}
