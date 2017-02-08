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

#include "remote-control-stub.h"
#include "remote-control.h"

remote_public
int remote_audio_get_state(void *priv, enum remote_audio_state *statep)
{
	enum RPC_TYPE(audio_state) state = RPC_MACRO(AUDIO_STATE_UNKNOWN);
	struct rpc_client *client = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int err;

	err = RPC_STUB(audio_get_state)(client, &ret, &state);
	if (err < 0)
		return err;

	/*
	 * TODO: Use a proper mapping between remote_mixer_input_source and
	 *       RPC_TYPE(mixer_input_source). This currently works because
	 *       both enumerations are defined identically.
	 */
	*statep = state;
	return ret;
}

remote_public
int remote_audio_set_state(void *priv, enum remote_audio_state state, bool force)
{
	struct rpc_client *client = rpc_client_from_priv(priv);
	enum RPC_TYPE(audio_state) input = state;
	int32_t ret = 0;
	int err;

	/*
	 * TODO: Use a proper mapping between remote_mixer_input_source and
	 *       RPC_TYPE(mixer_input_source). This currently works because
	 *       both enumerations are defined identically.
	 */
	err = RPC_STUB(audio_set_state)(client, &ret, input, force);
	if (err < 0)
		return err;

	return ret;
}

remote_public
int remote_audio_get_volume(void *priv, uint8_t *volume)
{
	struct rpc_client *client = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int err;

	err = RPC_STUB(audio_get_volume)(client, &ret, volume);
	if (err < 0)
		return err;

	return ret;
}

remote_public
int remote_audio_set_volume(void *priv, uint8_t volume)
{
	struct rpc_client *client = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int err;

	err = RPC_STUB(audio_set_volume)(client, &ret, volume);
	if (err < 0)
		return err;

	return ret;
}