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

int32_t RPC_IMPL(backlight_enable)(void *priv, uint32_t flags)
{
	struct backlight *backlight = remote_control_get_backlight(priv);
	int32_t ret;

	g_debug("> %s(priv=%p, flags=%x)", __func__, priv, flags);

	ret = backlight_enable(backlight, (flags & 0x1) == 0x1);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(backlight_set)(void *priv, uint8_t brightness)
{
	struct backlight *backlight = remote_control_get_backlight(priv);
	int32_t ret;

	g_debug("> %s(priv=%p, brightness=%02x)", __func__, priv, brightness);

	ret = backlight_set(backlight, brightness);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(backlight_get)(void *priv, uint8_t *brightness)
{
	struct backlight *backlight = remote_control_get_backlight(priv);
	int32_t ret;

	g_debug("> %s(priv=%p, brightness=%p)", __func__, priv, brightness);

	ret = backlight_get(backlight);
	if (ret >= 0) {
		*brightness = ret;
		ret = 0;
	}

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}
