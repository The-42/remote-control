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
int remote_mixer_set_mute(void *priv, enum remote_mixer_control control,
		bool mute)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int32_t err;

	err = medcom_mixer_set_mute_stub(rpc, &ret, control, mute);
	if (err < 0)
		ret = err;

	return ret;
}

remote_public
int remote_mixer_get_mute(void *priv, enum remote_mixer_control control,
		bool *mutep)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int32_t err;

	err = medcom_mixer_is_muted_stub(rpc, &ret, control, mutep);
	if (err < 0)
		ret = err;

	return ret;
}

remote_public
int remote_mixer_set_volume(void *priv, enum remote_mixer_control control,
		uint8_t volume)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int32_t err;

	err = medcom_mixer_set_volume_stub(rpc, &ret, control, volume);
	if (err < 0)
		ret = err;

	return ret;
}

remote_public
int remote_mixer_get_volume(void *priv, enum remote_mixer_control control,
		uint8_t *volumep)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int32_t err;

	err = medcom_mixer_get_volume_stub(rpc, &ret, control, volumep);
	if (err < 0)
		ret = err;

	return ret;
}

remote_public
int remote_mixer_set_input_source(void *priv,
		enum remote_mixer_input_source source)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	enum medcom_mixer_input_source input = source;
	int32_t ret = 0;
	int32_t err;

	/*
	 * TODO: Use a proper mapping between remote_mixer_input_source and
	 *       medcom_mixer_input_source. This currently works because both
	 *       enumerations are defined identically.
	 */

	err = medcom_mixer_set_input_source_stub(rpc, &ret, input);
	if (err < 0)
		ret = err;

	return ret;
}

remote_public
int remote_mixer_get_input_source(void *priv, enum remote_mixer_input_source *sourcep)
{
	enum medcom_mixer_input_source source = MEDCOM_MIXER_INPUT_SOURCE_UNKNOWN;
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int32_t err;

	err = medcom_mixer_get_input_source_stub(rpc, &ret, &source);
	if (err < 0)
		ret = err;

	/*
	 * TODO: Use a proper mapping between remote_mixer_input_source and
	 *       medcom_mixer_input_source. This currently works because both
	 *       enumerations are defined identically.
	 */
	*sourcep = source;
	return ret;
}

remote_public
int remote_mixer_loopback_enable(void *priv, bool enable)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int err;

	err = medcom_mixer_loopback_enable_stub(rpc, &ret, enable);
	if (err < 0)
		ret = err;

	return ret;
}

remote_public
int remote_mixer_loopback_is_enabled(void *priv, bool *enabled)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int err;

	err = medcom_mixer_loopback_is_enabled_stub(rpc, &ret, enabled);
	if (err < 0)
		ret = err;

	return ret;
}
