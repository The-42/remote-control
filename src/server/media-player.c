/*
 * Copyright (C) 2010-2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <glib.h>

#include "remote-control-stub.h"
#include "remote-control.h"

int32_t medcom_media_player_start(void *priv)
{
	struct media_player *player = remote_control_get_media_player(priv);
	int32_t ret;

	g_debug("> %s(priv=%p)", __func__, priv);

	ret = media_player_play(player);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_media_player_stop(void *priv)
{
	struct media_player *player = remote_control_get_media_player(priv);
	int32_t ret;

	g_debug("> %s(priv=%p)", __func__, priv);

	ret = media_player_stop(player);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_media_player_is_running(void *priv, bool *running)
{
	struct media_player *player = remote_control_get_media_player(priv);
	enum media_player_state state = MEDIA_PLAYER_STOPPED;
	int32_t ret = 0;
	int err;

	g_debug("> %s(priv=%p, running=%p)", __func__, priv, running);

	err = media_player_get_state(player, &state);
	if (err < 0) {
		ret = err;
		goto out;
	}

	switch (state) {
	case MEDIA_PLAYER_PLAYING:
		*running = true;
		break;

	default:
		*running = false;
		break;
	}

out:
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_media_player_set_stream(void *priv, const char *url)
{
	struct media_player *player = remote_control_get_media_player(priv);
	int32_t ret;

	g_debug("> %s(priv=%p, url=%s)", __func__, priv, url);

	ret = media_player_set_uri(player, url);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_media_player_get_stream(void *priv, char **url)
{
	struct media_player *player = remote_control_get_media_player(priv);

	return media_player_get_uri(player, url);
}

int32_t medcom_media_player_set_output_window(void *priv, uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
	struct media_player *player = remote_control_get_media_player(priv);
	int32_t ret;

	g_debug("> %s(priv=%p, x=%u, y=%u, width=%u, height=%u)", __func__,
			priv, x, y, width, height);

	ret = media_player_set_output_window(player, x, y, width, height);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}
