/*
 * Copyright (C) 2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "remote-control-stub.h"
#include "remote-control.h"

remote_public
int32_t remote_tuner_set_frequency(void *priv, unsigned long frequency)
{
	struct rpc_client *client = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int err;

	err = RPC_STUB(tuner_set_frequency)(client, &ret, frequency);
	if (err < 0)
		return err;

	return ret;
}
