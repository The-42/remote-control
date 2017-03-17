/*
 * Copyright (C) 2010-2012 Avionic Design GmbH
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
#include "rcrpc-client.h"

remote_public
int remote_sound_play(void *priv, const char *url)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret;
	int err;

	err = RPC_STUB(sound_play)(rpc, &ret, url);
	if (err < 0)
		return err;

	return ret;
}

remote_public
int remote_sound_stop(void *priv)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret;
	int err;

	err = RPC_STUB(sound_stop)(rpc, &ret);
	if (err < 0)
		return err;

	return ret;
}
