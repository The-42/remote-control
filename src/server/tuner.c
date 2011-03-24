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

int32_t RPC_IMPL(tuner_set_output_window)(void *priv, uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, x=%u, y=%u, width=%u, height=%u)", __func__,
			priv, x, y, width, height);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(tuner_set_output_channel)(void *priv, enum RPC_TYPE(tuner_channel) channel)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, channel=%d)", __func__, priv, channel);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(tuner_set_source)(void *priv, enum RPC_TYPE(tuner_source) source)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, source=%d)", __func__, priv, source);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(tuner_set_frequency)(void *priv, uint32_t frequency)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, frequency=%u)", __func__, priv, frequency);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}
