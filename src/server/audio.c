/*
 * src/server/audio.c (GENERIC AUDIO INTERFACE)
 *
 * Authors: Soeren Grunewald <soeren.grunewald@avionic-design.de>
 *
 * Copyright (C) 2010-2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "audio-backend.h"

int32_t RPC_IMPL(audio_set_state)(void *priv, enum RPC_TYPE(audio_state) state,
		bool force)
{
	struct audio* audio = remote_control_get_audio(priv);
	struct audio_context *ctx = audio_get_context(audio);
	struct audio_impl *impl = audio_get_impl(audio);
	int32_t ret = -ENOSYS;

	if (impl && impl->set_state)
		ret = impl->set_state(ctx, state, force);

	return ret;
}

int32_t RPC_IMPL(audio_get_state)(void *priv, enum RPC_TYPE(audio_state) *state)
{
	struct audio* audio = remote_control_get_audio(priv);
	struct audio_context *ctx = audio_get_context(audio);
	struct audio_impl *impl = audio_get_impl(audio);
	int32_t ret = -ENOSYS;

	if (impl && impl->get_state)
		ret = impl->get_state(ctx, state);

	return ret;
}

int32_t RPC_IMPL(audio_set_volume)(void *priv, uint8_t volume)
{
	struct audio* audio = remote_control_get_audio(priv);
	struct audio_context *ctx = audio_get_context(audio);
	struct audio_impl *impl = audio_get_impl(audio);
	int32_t ret = -ENOSYS;

	if (impl && impl->set_volume)
		ret = impl->set_volume(ctx, volume);

	return ret;
}

int32_t RPC_IMPL(audio_get_volume)(void *priv, uint8_t *volume)
{
	struct audio* audio = remote_control_get_audio(priv);
	struct audio_context *ctx = audio_get_context(audio);
	struct audio_impl *impl = audio_get_impl(audio);
	int32_t ret = -ENOSYS;

	if (impl && impl->get_volume)
		ret = impl->get_volume(ctx, volume);

	return ret;
}

int32_t RPC_IMPL(audio_enable_speakers)(void *priv, bool enable)
{
	struct audio* audio = remote_control_get_audio(priv);
	struct audio_context *ctx = audio_get_context(audio);
	struct audio_impl *impl = audio_get_impl(audio);
	int32_t ret = -ENOSYS;

	if (impl && impl->enable_speakers)
		ret = impl->enable_speakers(ctx, enable);

	return ret;
}

int32_t RPC_IMPL(audio_speakers_enabled)(void *priv, bool *enabled)
{
	struct audio* audio = remote_control_get_audio(priv);
	struct audio_context *ctx = audio_get_context(audio);
	struct audio_impl *impl = audio_get_impl(audio);
	int32_t ret = -ENOSYS;

	if (impl && impl->speakers_enabled)
		ret = impl->speakers_enabled(ctx, enabled);

	return ret;
}

int32_t RPC_IMPL(audio_call_function)(void *priv, uint32_t opcode, uint16_t param)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, opcode=%#x, param=%#x)", __func__, priv,
			opcode, param);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}