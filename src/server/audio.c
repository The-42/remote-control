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

int32_t RPC_IMPL(audio_set_state)(void *priv, enum RPC_TYPE(audio_state) state,
		bool force)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, state=%d, force=%d)", __func__, priv, state,
			force);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(audio_get_state)(void *priv, enum RPC_TYPE(audio_state) *state)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, state=%p)", __func__, priv, state);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(audio_set_volume)(void *priv, uint8_t volume)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, volume=%u)", __func__, priv, volume);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(audio_get_volume)(void *priv, uint8_t *volume)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, volume=%p)", __func__, priv, volume);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(audio_enable_speakers)(void *priv, bool enable)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, enable=%d)", __func__, priv, enable);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(audio_speakers_enabled)(void *priv, bool *enable)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, enable=%p)", __func__, priv, enable);
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
