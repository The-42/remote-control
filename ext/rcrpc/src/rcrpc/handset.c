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

#include <glib.h>

#include "remote-control-stub.h"
#include "remote-control-rpc.h"
#include "remote-control.h"

int32_t RPC_IMPL(handset_display_clear)(void *priv)
{
	struct handset *handset = remote_control_get_handset(RCPTR(priv));
	int32_t ret = 0;

	g_debug("> %s(priv=%p)", __func__, priv);

	ret = handset_display_clear(handset);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(handset_display_sync)(void *priv)
{
	struct handset *handset = remote_control_get_handset(RCPTR(priv));
	int32_t ret = 0;

	g_debug("> %s(priv=%p)", __func__, priv);

	ret = handset_display_sync(handset);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(handset_display_set_brightness)(void *priv,
		uint8_t brightness)
{
	struct handset *handset = remote_control_get_handset(RCPTR(priv));
	int32_t ret = 0;

	g_debug("> %s(priv=%p, brightness=%u)", __func__, priv, brightness);

	ret = handset_display_set_brightness(handset, brightness);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(handset_keypad_set_brightness)(void *priv,
		uint8_t brightness)
{
	struct handset *handset = remote_control_get_handset(RCPTR(priv));
	int32_t ret = 0;

	g_debug("> %s(priv=%p, brightness=%u)", __func__, priv, brightness);

	ret = handset_keypad_set_brightness(handset, brightness);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(handset_show_icon)(void *priv, enum RPC_TYPE(handset_icon) id)
{
	struct handset *handset = remote_control_get_handset(RCPTR(priv));
	int32_t ret = 0;

	g_debug("> %s(priv=%p, id=%d)", __func__, priv, id);

	ret = handset_icon_show(handset, id, true);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(handset_hide_icon)(void *priv, enum RPC_TYPE(handset_icon) id)
{
	struct handset *handset = remote_control_get_handset(RCPTR(priv));
	int32_t ret = 0;

	g_debug("> %s(priv=%p, id=%d)", __func__, priv, id);

	ret = handset_icon_show(handset, id, false);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(handset_show_text)(void *priv, uint32_t x, uint32_t y,
		const char *text)
{
	struct handset *handset = remote_control_get_handset(RCPTR(priv));
	int32_t ret = 0;

	g_debug("> %s(priv=%p, x=%u, y=%u, text=%p[%s])", __func__, priv, x,
			y, text, text);

	ret = handset_text_show(handset, x, y, text, true);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(handset_hide_text)(void *priv, uint32_t x, uint32_t y,
		const char *text)
{
	struct handset *handset = remote_control_get_handset(RCPTR(priv));
	int32_t ret = 0;

	g_debug("> %s(priv=%p, x=%u, y=%u, text=%p[%s])", __func__, priv, x,
			y, text, text);

	ret = handset_text_show(handset, x, y, text, false);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}
