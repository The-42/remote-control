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

#include <errno.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <vlc/vlc.h>

#include "remote-control-stub.h"
#include "remote-control.h"
#include "guri.h"

#define LIBVLC_AUDIO_VOLUME_MAX 200

struct media_player {
	enum media_player_state state;
	GdkWindow *window;

	libvlc_instance_t *vlc;
	libvlc_media_player_t *player;
	libvlc_event_manager_t *evman;
	libvlc_media_t *media;
};

static void on_playing(const struct libvlc_event_t *event, void *data)
{
	struct media_player *player = data;

	player->state = MEDIA_PLAYER_PLAYING;
}

static void on_stopped(const struct libvlc_event_t *event, void *data)
{
	struct media_player *player = data;

	GDK_THREADS_ENTER();
	player->state = MEDIA_PLAYER_STOPPED;
	gdk_window_hide(player->window);
	GDK_THREADS_LEAVE();
}

static void on_vout(const struct libvlc_event_t *event, void *data)
{
	struct media_player *player = data;

	if (event->u.media_player_vout.new_count > 0) {
		GDK_THREADS_ENTER();
		gdk_window_show(player->window);
		GDK_THREADS_LEAVE();
	}
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
#if GTK_CHECK_VERSION(2, 90, 5)
	cairo_region_t *region;
#else
	GdkRegion *region;
#endif
	XID xid;

	if (!playerp)
		return -EINVAL;

	player = g_new0(struct media_player, 1);
	if (!player)
		return -ENOMEM;

	player->state = MEDIA_PLAYER_STOPPED;

	player->window = gdk_window_new(NULL, &attributes, GDK_WA_NOREDIR);
	gdk_window_set_decorations(player->window, 0);
#if GTK_CHECK_VERSION(2, 91, 6)
	xid = gdk_x11_window_get_xid(player->window);
#else
	xid = gdk_x11_drawable_get_xid(player->window);
#endif

#if GTK_CHECK_VERSION(2, 90, 5)
	region = cairo_region_create();
#else
	region = gdk_region_new();
#endif
	gdk_window_input_shape_combine_region(player->window, region, 0, 0);
#if GTK_CHECK_VERSION(2, 90, 5)
	cairo_region_destroy(region);
#else
	gdk_region_destroy(region);
#endif

	player->vlc = libvlc_new(0, NULL);
	player->player = libvlc_media_player_new(player->vlc);
	libvlc_audio_set_volume(player->player, LIBVLC_AUDIO_VOLUME_MAX);
	player->evman = libvlc_media_player_event_manager(player->player);
	libvlc_media_player_set_xwindow(player->player, xid);

	libvlc_event_attach(player->evman, libvlc_MediaPlayerPlaying,
			on_playing, player);
	libvlc_event_attach(player->evman, libvlc_MediaPlayerStopped,
			on_stopped, player);
	libvlc_event_attach(player->evman, libvlc_MediaPlayerVout,
			on_vout, player);

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

	g_free(player);
	return 0;
}

int media_player_set_output_window(struct media_player *player,
		unsigned int x, unsigned int y, unsigned int width,
		unsigned int height)
{
	if (!player)
		return -EINVAL;

	gdk_window_move_resize(player->window, x, y, width, height);
	gdk_window_clear(player->window);

	return 0;
}

int media_player_set_uri(struct media_player *player, const char *uri)
{
	if (player->media)
		libvlc_media_release(player->media);

	if (uri) {
		/*
		 * URI is of format:
		 * URI option_1 option_x option_n
		 * split it at " " delimiter, to set options
		 */
		gchar **split_uri = g_strsplit(uri, " ", 0);
		gchar *location = split_uri[0];
		gchar **option = &split_uri[0];

		GURI *url = g_uri_new(location);
		const gchar *scheme;

		scheme = g_uri_get_scheme(url);
		if (!scheme) {
			g_strfreev(split_uri);
			g_object_unref(url);
			return -EINVAL;
		}

		if (g_str_equal(scheme, "udp")) {
			const gchar *host = g_uri_get_host(url);
			GInetAddress *address = g_inet_address_new_from_string(host);
			if (g_inet_address_get_is_multicast(address)) {
				/*
				 * HACK: Set user to empty string to force the
				 *       insertion of the @ separator.
				 */
				g_uri_set_user(url, "");
			}

			location = g_uri_to_string(url);
			g_object_unref(address);
		}

		gdk_window_hide(player->window);
		player->media = libvlc_media_new_location(player->vlc, location);

		if (location != split_uri[0])
			g_free(location);

		/*
		 * set media options, passed by the uri
		 */
		while (++option && *option)
			libvlc_media_add_option(player->media, *option);

		if (g_str_equal(scheme, "v4l2")) {
			/* TODO: autodetect the V4L2 and ALSA devices */
			libvlc_media_add_option(player->media, ":v4l2-dev=/dev/video0");
			libvlc_media_add_option(player->media, ":input-slave=alsa://hw:1");
			libvlc_video_set_deinterlace(player->player, NULL);
		} else {
			libvlc_video_set_deinterlace(player->player, "linear");
		}

		g_strfreev(split_uri);
		g_object_unref(url);
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
