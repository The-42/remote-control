/*
 * Copyright (C) 2010 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <errno.h>

#include <gdk/gdkscreen.h>
#include <gdk/gdkwindow.h>
#include <gdk/gdkx.h>
#include <vlc/vlc.h>

#include "remote-control-stub.h"
#include "remote-control.h"

struct media_player {
	enum media_player_state state;
	GdkWindow *window;

	libvlc_instance_t *vlc;
	libvlc_media_player_t *player;
	libvlc_event_manager_t *evman;
	libvlc_media_t *media;
};

static char *vlc_rewrite_url(const char *url)
{
	static const char scheme_separator[] = "://";
	size_t len;
	char *ptr;
	char *vlc;

	ptr = strstr(url, scheme_separator);
	if (!ptr)
		return NULL;

	ptr += strlen(scheme_separator);

	if (*ptr == '@')
		return strdup(url);

	len = strlen(url) + 2;

	vlc = g_malloc0(len);
	if (!vlc)
		return NULL;

	strncpy(vlc, url, ptr - url);
	strcat(vlc, "@");
	strcat(vlc, ptr);

	return vlc;
}

static void on_playing(const struct libvlc_event_t *event, void *data)
{
	struct media_player *player = data;
	g_debug("> %s(event=%p, data=%p)", __func__, event, data);
	gdk_threads_enter();
	player->state = MEDIA_PLAYER_PLAYING;
	gdk_window_show(player->window);
	gdk_threads_leave();
	g_debug("< %s()", __func__);
}

static void on_stopped(const struct libvlc_event_t *event, void *data)
{
	struct media_player *player = data;
	g_debug("> %s(event=%p, data=%p)", __func__, event, data);
	gdk_threads_enter();
	player->state = MEDIA_PLAYER_STOPPED;
	gdk_window_hide(player->window);
	gdk_threads_leave();
	g_debug("< %s()", __func__);
}

int media_player_create(struct media_player **playerp)
{
	GdkWindowAttr attributes = {
		.width = 320,
		.height = 240,
		.wclass = GDK_INPUT_OUTPUT,
		.window_type = GDK_WINDOW_CHILD,
		.override_redirect = TRUE,
	};
	struct media_player *player;
	XID xid;

	if (!playerp)
		return -EINVAL;

	player = malloc(sizeof(*player));
	if (!player)
		return -ENOMEM;

	memset(player, 0, sizeof(*player));
	player->state = MEDIA_PLAYER_STOPPED;

	player->window = gdk_window_new(NULL, &attributes, GDK_WA_NOREDIR);
	xid = gdk_x11_drawable_get_xid(player->window);
	gdk_window_set_decorations(player->window, 0);

	player->vlc = libvlc_new(0, NULL);
	player->player = libvlc_media_player_new(player->vlc);
	player->evman = libvlc_media_player_event_manager(player->player);
	libvlc_media_player_set_xwindow(player->player, xid);

	libvlc_event_attach(player->evman, libvlc_MediaPlayerPlaying,
			on_playing, player);
	libvlc_event_attach(player->evman, libvlc_MediaPlayerStopped,
			on_stopped, player);

	*playerp = player;
	return 0;
}

int media_player_free(struct media_player *player)
{
	if (!player)
		return -EINVAL;

	libvlc_media_player_release(player->player);
	libvlc_media_release(player->media);
	libvlc_release(player->vlc);

	gdk_window_destroy(player->window);

	free(player);
	return 0;
}

int media_player_set_output_window(struct media_player *player,
		unsigned int x, unsigned int y, unsigned int width,
		unsigned int height)
{
	if (!player)
		return -EINVAL;

	gdk_window_move_resize(player->window, x, y, width, height);

	return 0;
}

int media_player_set_uri(struct media_player *player, const char *uri)
{
	if (player->media)
		libvlc_media_release(player->media);

	if (uri) {
		char *url = vlc_rewrite_url(uri);
		if (!url)
			return -EINVAL;

		gdk_window_hide(player->window);
		player->media = libvlc_media_new_location(player->vlc, url);

		g_free(url);
	}

	return 0;
}

int media_player_get_uri(struct media_player *player, char **urip)
{
	if (!player || !player->media || !urip)
		return -EINVAL;

	*urip = libvlc_media_get_mrl(player->media);
	return 0;
}

int media_player_play(struct media_player *player)
{
	libvlc_media_player_set_media(player->player, player->media);
	libvlc_media_player_play(player->player);
	return 0;
}

int media_player_stop(struct media_player *player)
{
	libvlc_media_player_stop(player->player);
	return 0;
}

int media_player_get_state(struct media_player *player,
		enum media_player_state *statep)
{
	if (!player || !statep)
		return -EINVAL;

	*statep = player->state;
	return 0;
}
