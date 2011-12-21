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

int32_t RPC_IMPL(audio_set_state)(void *priv, enum RPC_TYPE(audio_state) state,
		bool force)
{
	struct audio *audio = remote_control_get_audio(priv);
	enum audio_state as;
	int32_t ret;

	g_debug("> %s(priv=%p, state=%d, force=%d)", __func__, priv, state,
			force);

	switch (state) {
	case RPC_MACRO(AUDIO_STATE_INACTIVE):
		as = AUDIO_STATE_INACTIVE;
		break;

	case RPC_MACRO(AUDIO_STATE_HIFI_PLAYBACK_SPEAKER):
		as = AUDIO_STATE_HIFI_PLAYBACK_SPEAKER;
		break;

	case RPC_MACRO(AUDIO_STATE_HIFI_PLAYBACK_HEADSET):
		as = AUDIO_STATE_HIFI_PLAYBACK_HEADSET;
		break;

	case RPC_MACRO(AUDIO_STATE_VOICECALL_HANDSET):
		as = AUDIO_STATE_VOICECALL_HANDSET;
		break;

	case RPC_MACRO(AUDIO_STATE_VOICECALL_HEADSET):
		as = AUDIO_STATE_VOICECALL_HEADSET;
		break;

	case RPC_MACRO(AUDIO_STATE_VOICECALL_SPEAKER):
		as = AUDIO_STATE_VOICECALL_SPEAKER;
		break;

	case RPC_MACRO(AUDIO_STATE_VOICECALL_IP_HANDSET):
		as = AUDIO_STATE_VOICECALL_IP_HANDSET;
		break;

	case RPC_MACRO(AUDIO_STATE_VOICECALL_IP_HEADSET):
		as = AUDIO_STATE_VOICECALL_IP_HEADSET;
		break;

	case RPC_MACRO(AUDIO_STATE_VOICECALL_IP_SPEAKER):
		as = AUDIO_STATE_VOICECALL_IP_SPEAKER;
		break;

	default:
		ret = -EINVAL;
		goto out;
	}

	ret = audio_set_state(audio, as);

out:
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(audio_get_state)(void *priv, enum RPC_TYPE(audio_state) *statep)
{
	struct audio *audio = remote_control_get_audio(priv);
	enum audio_state as;
	int32_t ret;

	g_debug("> %s(priv=%p, state=%p)", __func__, priv, statep);

	ret = audio_get_state(audio, &as);
	if (ret < 0)
		goto out;

	switch (as) {
	case AUDIO_STATE_INACTIVE:
		*statep = RPC_MACRO(AUDIO_STATE_INACTIVE);
		break;

	case AUDIO_STATE_HIFI_PLAYBACK_SPEAKER:
		*statep = RPC_MACRO(AUDIO_STATE_HIFI_PLAYBACK_SPEAKER);
		break;

	case AUDIO_STATE_HIFI_PLAYBACK_HEADSET:
		*statep = RPC_MACRO(AUDIO_STATE_HIFI_PLAYBACK_HEADSET);
		break;

	case AUDIO_STATE_VOICECALL_HANDSET:
		*statep = RPC_MACRO(AUDIO_STATE_VOICECALL_HANDSET);
		break;

	case AUDIO_STATE_VOICECALL_HEADSET:
		*statep = RPC_MACRO(AUDIO_STATE_VOICECALL_HEADSET);
		break;

	case AUDIO_STATE_VOICECALL_SPEAKER:
		*statep = RPC_MACRO(AUDIO_STATE_VOICECALL_SPEAKER);
		break;

	case AUDIO_STATE_VOICECALL_IP_HANDSET:
		*statep = RPC_MACRO(AUDIO_STATE_VOICECALL_IP_HANDSET);
		break;

	case AUDIO_STATE_VOICECALL_IP_HEADSET:
		*statep = RPC_MACRO(AUDIO_STATE_VOICECALL_IP_HEADSET);
		break;

	case AUDIO_STATE_VOICECALL_IP_SPEAKER:
		*statep = RPC_MACRO(AUDIO_STATE_VOICECALL_IP_SPEAKER);
		break;

	default:
		*statep = RPC_MACRO(AUDIO_STATE_UNKNOWN);
		break;
	}

out:
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(audio_set_volume)(void *priv, uint8_t volume)
{
	struct audio *audio = remote_control_get_audio(priv);
	int32_t ret;

	g_debug("> %s(priv=%p, volume=%u)", __func__, priv, volume);

	ret = audio_set_volume(audio, volume);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(audio_get_volume)(void *priv, uint8_t *volumep)
{
	struct audio *audio = remote_control_get_audio(priv);
	int32_t ret;

	g_debug("> %s(priv=%p, volumep=%p)", __func__, priv, volumep);

	ret = audio_get_volume(audio, volumep);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(audio_enable_speakers)(void *priv, bool enable)
{
	struct audio* audio = remote_control_get_audio(priv);
	int32_t ret;

	g_debug("> %s(priv=%p, enable=%d)", __func__, priv, enable);

	ret = audio_set_speakers_enable(audio, enable);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(audio_speakers_enabled)(void *priv, bool *enablep)
{
	struct audio* audio = remote_control_get_audio(priv);
	int32_t ret;

	g_debug("> %s(priv=%p, enablep=%p)", __func__, priv, enablep);

	ret = audio_get_speakers_enable(audio, enablep);

	g_debug("< %s() = %d", __func__, ret);
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
