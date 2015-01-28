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

#ifdef ENABLE_LIBTNJ3324
#include <tnj3324.h>

#define TNJ3324_DEFAULT_I2CBUS 0
#endif

#include "remote-control-stub.h"
#include "remote-control.h"
#include "guri.h"

struct options {
	gint64 buffer_duration;
};

struct media_player {
	enum media_player_state state;
	GdkWindow *window;

	libvlc_instance_t *vlc;
	libvlc_media_player_t *player;
	libvlc_event_manager_t *evman;
	libvlc_media_t *media;

#ifdef ENABLE_LIBTNJ3324
	struct tnj3324_t tnj3324;
#endif

	struct options opts;

	media_player_es_changed_cb es_changed_cb;
	void *es_changed_data;
};

static gboolean hide_window(gpointer data)
{
	gdk_window_hide(GDK_WINDOW(data));
	return FALSE;
}

static gboolean show_window(gpointer data)
{
	gdk_window_show(GDK_WINDOW(data));
	return FALSE;
}

static void on_playing(const struct libvlc_event_t *event, void *data)
{
	struct media_player *player = data;

	player->state = MEDIA_PLAYER_PLAYING;
}

static void on_stopped(const struct libvlc_event_t *event, void *data)
{
	struct media_player *player = data;

	g_idle_add(hide_window, player->window);
	player->state = MEDIA_PLAYER_STOPPED;
}

static void on_paused(const struct libvlc_event_t *event, void *data)
{
	struct media_player *player = data;

	player->state = MEDIA_PLAYER_PAUSED;
}

static void on_vout(const struct libvlc_event_t *event, void *data)
{
	struct media_player *player = data;

	if (event->u.media_player_vout.new_count > 0)
		g_idle_add(show_window, player->window);
}

static void on_es_changed(const struct libvlc_event_t *event, void *data)
{
	media_player_es_changed_cb es_changed_cb;
	struct media_player *player = data;
	enum media_player_es_action action;
	enum media_player_es_type type;
	void *es_changed_data;
	int pid;

	if (!player)
		return;

	es_changed_cb = player->es_changed_cb;
	es_changed_data = player->es_changed_data;

	if (!es_changed_cb)
		return;

	switch (event->type) {
		case libvlc_MediaPlayerESAdded:
			action = MEDIA_PLAYER_ES_ADDED;
			break;
		case libvlc_MediaPlayerESDeleted:
			action = MEDIA_PLAYER_ES_DELETED;
			break;
		default:
			return;
	}
	switch (event->u.media_player_es_changed.i_type) {
		case libvlc_track_audio:
			type = MEDIA_PLAYER_ES_AUDIO;
			break;
		case libvlc_track_video:
			type = MEDIA_PLAYER_ES_VIDEO;
			break;
		case libvlc_track_text:
			type = MEDIA_PLAYER_ES_TEXT;
			break;
		default:
			type = MEDIA_PLAYER_ES_UNKNOWN;
	}
	pid = event->u.media_player_es_changed.i_id;
	es_changed_cb(es_changed_data, action, type, pid);
}

int media_player_create(struct media_player **playerp, GKeyFile *config)
{
	GdkWindowAttr attributes = {
		.width = 320,
		.height = 240,
		.wclass = GDK_INPUT_OUTPUT,
		.window_type = GDK_WINDOW_CHILD,
		.override_redirect = TRUE,
	};
	const char *argv[] = { "--no-osd" };
	struct media_player *player;
#if GTK_CHECK_VERSION(2, 90, 5)
	cairo_region_t *region;
#else
	GdkRegion *region;
#endif
#ifdef ENABLE_LIBTNJ3324
	int i2cbus;
#endif
	GError *err = NULL;
	gint64 duration;
	int volume;
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

	player->vlc = libvlc_new(G_N_ELEMENTS(argv), argv);
	player->player = libvlc_media_player_new(player->vlc);
	player->evman = libvlc_media_player_event_manager(player->player);
	libvlc_media_player_set_xwindow(player->player, xid);

	volume = g_key_file_get_integer(config, "media-player", "volume", &err);
	if (err) {
		g_clear_error(&err);
		volume = 100;
	}
	if (libvlc_audio_set_volume(player->player, volume))
		g_warning("Failed to set media player VLC base volume to %d", volume);
	else
		g_info("Set media player VLC base volume to %d", volume);

	libvlc_event_attach(player->evman, libvlc_MediaPlayerPlaying,
			on_playing, player);
	libvlc_event_attach(player->evman, libvlc_MediaPlayerStopped,
			on_stopped, player);
	libvlc_event_attach(player->evman, libvlc_MediaPlayerPaused,
			on_paused, player);
	libvlc_event_attach(player->evman, libvlc_MediaPlayerVout,
			on_vout, player);
	libvlc_event_attach(player->evman, libvlc_MediaPlayerESAdded,
			on_es_changed, player);
	libvlc_event_attach(player->evman, libvlc_MediaPlayerESDeleted,
			on_es_changed, player);

#ifdef ENABLE_LIBTNJ3324
	i2cbus = g_key_file_get_integer(config, "media-player",
	                                "tnj3324-i2cbus", &err);
	if (err) {
		g_clear_error(&err);
		i2cbus = TNJ3324_DEFAULT_I2CBUS;
	}
	tnj3324_init(&player->tnj3324, i2cbus);
#endif

	duration = g_key_file_get_int64(config, "media-player",
	                                "buffer-duration", &err);
	if (err) {
		g_clear_error(&err);
		duration = -1;
	}
	player->opts.buffer_duration = duration;

	*playerp = player;
	return 0;
}

int media_player_free(struct media_player *player)
{
	g_return_val_if_fail(player != NULL, -EINVAL);

	libvlc_media_player_release(player->player);
	libvlc_media_release(player->media);
	libvlc_release(player->vlc);

	gdk_window_destroy(player->window);

	g_free(player);
	return 0;
}

int media_player_set_crop(struct media_player *player,
		unsigned int left, unsigned int right, unsigned int top,
		unsigned int bottom)
{
	return -ENOSYS;
}

int media_player_set_output_window(struct media_player *player,
		unsigned int x, unsigned int y, unsigned int width,
		unsigned int height)
{
	g_return_val_if_fail(player != NULL, -EINVAL);

	gdk_window_move_resize(player->window, x, y, width, height);
	gdk_window_clear(player->window);

	return 0;
}

int media_player_set_uri(struct media_player *player, const char *uri)
{
	const gchar *scheme;
	gchar **split_uri;
	gint64 duration;
	gchar *location;
	gchar **option;
	GURI *url;

	g_return_val_if_fail(player != NULL, -EINVAL);
	g_return_val_if_fail(uri != NULL, -EINVAL);

	if (player->media) {
		libvlc_media_release(player->media);
		player->media = NULL;
	}

	/*
	 * URI is of format:
	 * URI option_1 option_x option_n
	 * split it at " " delimiter, to set options
	 */
	split_uri = g_strsplit(uri, " ", 0);
	location = split_uri[0];

	url = g_uri_new(location);
	if (!url) {
		g_strfreev(split_uri);
		return -ENOMEM;
	}

	scheme = g_uri_get_scheme(url);
	if (!scheme) {
		g_warning("media-player: %s is not a valid uri", uri);
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

	if (location != split_uri[0]) {
		g_free(location);
		location = NULL;
	}

	duration = player->opts.buffer_duration;
	/*
	 * set media options, passed by the uri
	 */
	option = &split_uri[0];
	while (++option && *option) {
#ifdef ENABLE_LIBTNJ3324
		if (g_str_equal(scheme, "v4l2") &&
		    g_str_has_prefix(*option ,":frequency=")) {
			const gchar *value = g_strrstr(*option, "=") + 1;
			float freq = (float)g_ascii_strtoll(value, NULL, 10) / 1000;

			g_debug("   select tnj3324 input frequency = %.02f\n", freq);
			tnj3324_set_frequency(&player->tnj3324, freq);
			continue;
		}
#endif
		/* uri option overrides config entry */
		if (g_str_has_prefix(*option, ":network-caching=")) {
			const gchar *value = g_strrstr(*option, "=") + 1;
			duration = (gint64)g_ascii_strtoll(value, NULL, 10);
		} else
			libvlc_media_add_option(player->media, *option);
	}

	if (duration != -1) {
		char opt[32];
		snprintf(opt, sizeof(opt), ":network-caching=%" G_GINT64_FORMAT,
			 duration);
		libvlc_media_add_option(player->media, opt);
		g_debug("   appended option: %s", opt);
	}

	if (g_str_equal(scheme, "v4l2")) {
		/* TODO: autodetect the V4L2 and ALSA devices */
		libvlc_media_add_option(player->media, ":v4l2-dev=/dev/video0");
		/* FIXME: this will not work with usb handset */
		libvlc_media_add_option(player->media, ":input-slave=alsa://hw:1");
		libvlc_video_set_deinterlace(player->player, NULL);
	} else {
#if defined(__arm__)
		libvlc_video_set_deinterlace(player->player, "blend");
#else
		libvlc_video_set_deinterlace(player->player, "linear");
#endif
	}

	g_strfreev(split_uri);
	g_object_unref(url);

	return 0;
}

int media_player_get_uri(struct media_player *player, char **urip)
{
	g_return_val_if_fail(player != NULL, -EINVAL);
	g_return_val_if_fail(player->media != NULL, -EINVAL);

	if (urip)
		*urip = libvlc_media_get_mrl(player->media);

	return 0;
}

int media_player_play(struct media_player *player)
{
	g_return_val_if_fail(player != NULL, -EINVAL);

	libvlc_media_player_set_media(player->player, player->media);
	libvlc_media_player_play(player->player);
	return 0;
}

int media_player_stop(struct media_player *player)
{
	g_return_val_if_fail(player != NULL, -EINVAL);

	libvlc_media_player_stop(player->player);
	return 0;
}

int media_player_pause(struct media_player *player)
{
	g_return_val_if_fail(player != NULL, -EINVAL);

	if (!libvlc_media_player_can_pause(player->player))
		return -ENOTSUP;

	libvlc_media_player_set_pause(player->player, TRUE);
	player->state = MEDIA_PLAYER_PAUSED;

	return 0;
}

int media_player_resume(struct media_player *player)
{
	g_return_val_if_fail(player != NULL, -EINVAL);

	if (!libvlc_media_player_can_pause(player->player))
		return -ENOTSUP;

	libvlc_media_player_set_pause(player->player, FALSE);

	return 0;
}

int media_player_get_duration(struct media_player *player,
		unsigned long *duration)
{
	libvlc_time_t time;

	g_return_val_if_fail(player != NULL, -EINVAL);

	time = libvlc_media_player_get_length(player->player);
	if (time < 0)
		return -ENODATA;

	/* time is in milliseconds */
	if (duration)
		*duration = time / 1000;

	return 0;
}

int media_player_get_position(struct media_player *player,
		unsigned long *position)
{
	libvlc_time_t time;

	g_return_val_if_fail(player != NULL, -EINVAL);

	time = libvlc_media_player_get_time(player->player);
	if (time < 0)
		return -ENODATA;

	/* time is in milliseconds */
	if (position)
		*position = time / 1000;

	return 0;
}

int media_player_set_position(struct media_player *player,
		unsigned long position)
{
	/* time is in milliseconds */
	libvlc_time_t time = position * 1000;

	g_return_val_if_fail(player != NULL, -EINVAL);

	if (!libvlc_media_player_is_seekable(player->player))
		return -ENOTSUP;

	libvlc_media_player_set_time(player->player, time);

	return 0;
}

int media_player_get_state(struct media_player *player,
		enum media_player_state *statep)
{
	g_return_val_if_fail(player != NULL, -EINVAL);

	if (statep)
		*statep = player->state;

	return 0;
}

int media_player_get_mute(struct media_player *player, bool *mute)
{
	int muted;

	g_return_val_if_fail(player != NULL, -EINVAL);

	muted = libvlc_audio_get_mute(player->player);
	if (mute)
		*mute = !!muted;
	return 0;
}

int media_player_set_mute(struct media_player *player, bool mute)
{
	g_return_val_if_fail(player != NULL, -EINVAL);

	libvlc_audio_set_mute(player->player, mute);
	return 0;
}

int media_player_get_audio_track_count(struct media_player *player, int *count)
{
	g_return_val_if_fail(player != NULL, -EINVAL);
	g_return_val_if_fail(count != NULL, -EINVAL);

	*count = libvlc_audio_get_track_count(player->player);
	if (*count < 0)
		*count = 0;

	return 0;
}

int media_player_get_audio_track_pid(struct media_player *player, int pos, int *pid)
{
	libvlc_track_description_t *track_list;
	libvlc_track_description_t *track;
	int ret = -ENOENT;

	g_return_val_if_fail(player != NULL, -EINVAL);
	g_return_val_if_fail(pid != NULL, -EINVAL);

	track_list = libvlc_audio_get_track_description(player->player);
	track = track_list;
	while (track && pos-- > 0)
		track = track->p_next;

	if (!track)
		goto cleanup;

	*pid = track->i_id;
	ret = 0;
cleanup:
	if (track_list)
		libvlc_track_description_list_release(track_list);
	return ret;
}

int media_player_get_audio_track_name(struct media_player *player, int pid, char **name)
{
	libvlc_track_description_t *track_list;
	libvlc_track_description_t *track;
	int ret = 0;

	if (!name)
		return -EINVAL;

	track_list = libvlc_audio_get_track_description(player->player);
	track = track_list;
	while (track) {
		if (pid == track->i_id) {
			*name = g_strdup(track->psz_name);
			goto cleanup;
		}
		track = track->p_next;
	}
	ret = -ENOENT;
cleanup:
	if (track_list)
		libvlc_track_description_list_release(track_list);
	return ret;
}

int media_player_get_audio_track(struct media_player *player, int *pid)
{
	g_return_val_if_fail(player != NULL, -EINVAL);
	g_return_val_if_fail(pid != NULL, -EINVAL);

	*pid = libvlc_audio_get_track(player->player);

	return 0;
}

int media_player_set_audio_track(struct media_player *player, int pid)
{
	g_return_val_if_fail(player != NULL, -EINVAL);

	return libvlc_audio_set_track(player->player, pid);
}

int media_player_get_spu_count(struct media_player *player, int *count)
{
	g_return_val_if_fail(player != NULL, -EINVAL);
	g_return_val_if_fail(count != NULL, -EINVAL);

	*count = libvlc_video_get_spu_count(player->player);
	if (*count < 0)
		*count = 0;

	return 0;
}

int media_player_get_spu_pid(struct media_player *player, int pos, int *pid)
{
	libvlc_track_description_t *track_list;
	libvlc_track_description_t *track;
	int ret = -ENOENT;

	g_return_val_if_fail(player != NULL, -EINVAL);
	g_return_val_if_fail(pid != NULL, -EINVAL);

	track_list = libvlc_video_get_spu_description(player->player);
	track = track_list;
	while (track && pos-- > 0)
		track = track->p_next;

	if (!track)
		goto cleanup;

	*pid = track->i_id;
	ret = 0;
cleanup:
	if (track_list)
		libvlc_track_description_list_release(track_list);
	return ret;
}

int media_player_get_spu_name(struct media_player *player, int pid, char **name)
{
	libvlc_track_description_t *track_list;
	libvlc_track_description_t *track;
	int ret = 0;

	track_list = libvlc_video_get_spu_description(player->player);
	track = track_list;
	while (track) {
		if (pid == track->i_id) {
			*name = g_strdup(track->psz_name);
			goto cleanup;
		}
		track = track->p_next;
	}
	ret = -ENOENT;
cleanup:
	if (track_list)
		libvlc_track_description_list_release(track_list);
	return ret;
}

int media_player_get_spu(struct media_player *player, int *pid)
{
	g_return_val_if_fail(player != NULL, -EINVAL);
	g_return_val_if_fail(pid != NULL, -EINVAL);

	*pid = libvlc_video_get_spu(player->player);

	return 0;
}

int media_player_set_spu(struct media_player *player, int pid)
{
	g_return_val_if_fail(player != NULL, -EINVAL);

	return libvlc_video_set_spu(player->player, pid);
}

int media_player_get_teletext(struct media_player *player, int *page)
{
	g_return_val_if_fail(player != NULL, -EINVAL);
	g_return_val_if_fail(page != NULL, -EINVAL);

	*page = libvlc_video_get_teletext(player->player);

	return 0;
}

int media_player_set_teletext(struct media_player *player, int page)
{
	g_return_val_if_fail(player != NULL, -EINVAL);

	libvlc_video_set_teletext(player->player, page);

	return 0;
}

int media_player_toggle_teletext_transparent(struct media_player *player)
{
	g_return_val_if_fail(player != NULL, -EINVAL);

	libvlc_toggle_teletext(player->player);

	return 0;
}

int media_player_set_es_changed_callback(struct media_player *player,
		media_player_es_changed_cb callback, void *data)
{
	g_return_val_if_fail(player != NULL, -EINVAL);
	if (!callback) {
		/* Only clear callback if it is the current owner */
		if (player->es_changed_data == data) {
			player->es_changed_cb = NULL;
			player->es_changed_data = NULL;
		}
	} else {
		player->es_changed_data = data;
		player->es_changed_cb = callback;
	}
	return 0;
}
