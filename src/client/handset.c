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
int remote_handset_display_clear(void *priv)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int32_t err;

	err = RPC_STUB(handset_display_clear)(rpc, &ret);
	if (err < 0)
		ret = err;

	return ret;
}

remote_public
int remote_handset_display_sync(void *priv)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int32_t err;

	err = RPC_STUB(handset_display_sync)(rpc, &ret);
	if (err < 0)
		ret = err;

	return ret;
}

remote_public
int remote_handset_display_set_brightness(void *priv, unsigned int brightness)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int32_t err;

	err = RPC_STUB(handset_display_set_brightness)(rpc, &ret, brightness);
	if (err < 0)
		ret = err;

	return ret;
}

remote_public
int remote_handset_keypad_set_brightness(void *priv, unsigned int brightness)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int32_t err;

	err = RPC_STUB(handset_keypad_set_brightness)(rpc, &ret, brightness);
	if (err < 0)
		ret = err;

	return ret;
}

remote_public
int remote_handset_show_icon(void *priv, enum remote_handset_icon id)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int32_t err;

	err = RPC_STUB(handset_show_icon)(rpc, &ret, id);
	if (err < 0)
		ret = err;

	return ret;
}

remote_public
int remote_handset_hide_icon(void *priv, enum remote_handset_icon id)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int32_t err;

	err = RPC_STUB(handset_hide_icon)(rpc, &ret, id);
	if (err < 0)
		ret = err;

	return ret;
}

remote_public
int remote_handset_show_text(void *priv, unsigned int x, unsigned int y,
		const char *text)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int32_t err;

	err = RPC_STUB(handset_show_text)(rpc, &ret, x, y, text);
	if (err < 0)
		ret = err;

	return ret;
}

remote_public
int remote_handset_hide_text(void *priv, unsigned int x, unsigned int y,
		const char *text)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int32_t err;

	err = RPC_STUB(handset_hide_text)(rpc, &ret, x, y, text);
	if (err < 0)
		ret = err;

	return ret;
}
