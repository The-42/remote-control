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

int32_t RPC_IMPL(media_player_start)(void *priv)
{
	struct media_player *player = remote_control_get_media_player(RCPTR(priv));
	int32_t ret;

	g_debug("> %s(priv=%p)", __func__, priv);

	ret = media_player_play(player);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(media_player_stop)(void *priv)
{
	struct media_player *player = remote_control_get_media_player(RCPTR(priv));
	int32_t ret;

	g_debug("> %s(priv=%p)", __func__, priv);

	ret = media_player_stop(player);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(media_player_is_running)(void *priv, bool *running)
{
	struct media_player *player = remote_control_get_media_player(RCPTR(priv));
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

int32_t RPC_IMPL(media_player_set_stream)(void *priv, const char *url)
{
	struct media_player *player = remote_control_get_media_player(RCPTR(priv));
	int32_t ret;

	g_debug("> %s(priv=%p, url=%s)", __func__, priv, url);

	ret = media_player_set_uri(player, url);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(media_player_get_stream)(void *priv, char **url)
{
	struct media_player *player = remote_control_get_media_player(RCPTR(priv));

	return media_player_get_uri(player, url);
}

int32_t RPC_IMPL(media_player_set_crop)(void *priv, uint16_t left,
			uint16_t right, uint16_t top, uint16_t bottom)
{
	struct media_player *player = remote_control_get_media_player(RCPTR(priv));
	int32_t ret;

	g_debug("> %s(priv=%p, left=%u, right=%u, top=%u, bottom=%u)", __func__,
			priv, left, right, top, bottom);

	ret = media_player_set_crop(player, left, right, top, bottom);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(media_player_set_output_window)(void *priv, uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
	struct media_player *player = remote_control_get_media_player(RCPTR(priv));
	int32_t ret;

	g_debug("> %s(priv=%p, x=%u, y=%u, width=%u, height=%u)", __func__,
			priv, x, y, width, height);

	ret = media_player_set_output_window(player, x, y, width, height);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(media_player_pause)(void *priv)
{
	struct media_player *player = remote_control_get_media_player(RCPTR(priv));
	int32_t ret;

	g_debug("> %s(priv=%p)", __func__, priv);

	ret = media_player_pause(player);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(media_player_resume)(void *priv)
{
	struct media_player *player = remote_control_get_media_player(RCPTR(priv));
	int32_t ret;

	g_debug("> %s(priv=%p)", __func__, priv);

	ret = media_player_resume(player);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(media_player_get_duration)(void *priv, uint32_t *duration)
{
	struct media_player *player = remote_control_get_media_player(RCPTR(priv));
	unsigned long time = 0;
	int32_t ret;

	g_debug("> %s(priv=%p, duration=%p)", __func__, priv, duration);

	ret = media_player_get_duration(player, &time);

	if ((ret >= 0) && duration)
		*duration = time;

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(media_player_get_position)(void *priv, uint32_t *position)
{
	struct media_player *player = remote_control_get_media_player(RCPTR(priv));
	unsigned long time = 0;
	int32_t ret;

	g_debug("> %s(priv=%p, position=%p)", __func__, priv, position);

	ret = media_player_get_position(player, &time);

	if ((ret >= 0) && position)
		*position = time;

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(media_player_set_position)(void *priv, uint32_t position)
{
	struct media_player *player = remote_control_get_media_player(RCPTR(priv));
	int32_t ret;

	g_debug("> %s(priv=%p, position=%u)", __func__, priv, position);

	ret = media_player_set_position(player, position);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}
