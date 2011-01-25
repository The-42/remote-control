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
#include <gdk/gdkscreen.h>
#include <gdk/gdkwindow.h>
#include <gdk/gdkx.h>

#include <gst/gst.h>
#include <gst/interfaces/xoverlay.h>

#include "remote-control-stub.h"
#include "remote-control.h"

#define LIBVLC_AUDIO_VOLUME_MAX 200

struct media_player {
	enum media_player_state state;
	GstElement *pipeline;
	GdkWindow *window;  /* the window to use for output */
	XID xid;            /* the id of the x window */
	gboolean verbose;
};



static enum media_player_state player_gst_state_2_media_state(GstState state)
{
	switch (state) {
	case GST_STATE_VOID_PENDING:
	case GST_STATE_NULL:
	case GST_STATE_READY:
	default:
		return MEDIA_PLAYER_STOPPED;
	case GST_STATE_PAUSED:
		return MEDIA_PLAYER_PAUSED;
	case GST_STATE_PLAYING:
		return MEDIA_PLAYER_PLAYING;
	}
}

static int player_error_message(struct media_player *player, GstMessage *message)
{
	GError *err;
	gchar *debug;

	gst_message_parse_error (message, &err, &debug);
	g_print ("Error: %s\n", err->message);
	g_error_free (err);
	g_free (debug);

	return 0;
}

static int player_handle_stream_status(struct media_player *player, GstMessage *message)
{
	GstStreamStatusType type = GST_STREAM_STATUS_TYPE_CREATE;

	gst_message_parse_stream_status(message, &type, NULL);
	g_print("   new stream status is: %d\n", type);

	return 0;
}

static int player_handle_state_changed(struct media_player *player, GstMessage *message)
{
	GstState old_state, new_state;

	gst_message_parse_state_changed(message, &old_state, &new_state, NULL);
	g_print("   Element %s changed state from %s to %s.\n",
		GST_OBJECT_NAME(message->src),
		gst_element_state_get_name(old_state),
		gst_element_state_get_name(new_state));

	return 0;
}

static void player_dump_tags(const GstTagList *list, const gchar *tag, gpointer user_data)
{
	char *value = NULL;

	if (gst_tag_list_get_string(list, tag, &value)) {
		g_print("  TAG: %s = %s\n", tag, value);
	}

	g_free(value);
}

static int player_handle_tag(struct media_player *player, GstMessage *message)
{
	GstTagList *tag_list = NULL;

	gst_message_parse_tag(message, &tag_list);
	gst_tag_list_foreach(tag_list, player_dump_tags, player);
	gst_tag_list_free(tag_list);

	return 0;
}

static GstBusSyncReply player_bus_sync_handler(GstBus *bus, GstMessage *message,
		gpointer user_data)
{
	struct media_player *player = user_data;
	GstMessageType type = GST_MESSAGE_TYPE(message);
	GstBusSyncReply ret = GST_BUS_PASS;

	switch (type) {
	case GST_MESSAGE_ELEMENT:
		if (!gst_structure_has_name(message->structure, "prepare-xwindow-id"))
			break;

		g_print("**  showing window...\n");
		gdk_threads_enter();
		g_print("    looked\n");
		gdk_window_show(player->window);
		g_print("    show\n");
		gdk_threads_leave();
		g_print("    unlooked\n");

		g_print("**  try to get XID...\n");
		if (player && player->xid != 0) {
			GstXOverlay *overlay = GST_X_OVERLAY(GST_MESSAGE_SRC(message));
			g_object_set(overlay, "force-aspect-ratio", TRUE, NULL);
			player->xid = gdk_x11_drawable_get_xid(player->window);
			g_print("**  XID is: %lu\n", player->xid);
			gst_x_overlay_set_xwindow_id(overlay, player->xid);
		} else {
			g_print("**  %p:%lu: no window XID yet!\n", player, player->xid);
		}

		gst_message_unref(message);
		ret = GST_BUS_DROP;
		break;

	case GST_MESSAGE_ERROR:
		ret = player_error_message(player, message);
	case GST_MESSAGE_EOS:
		gdk_threads_enter();
		gdk_window_hide(player->window);
		gdk_threads_leave();
		g_print("**  hiding window\n");
		break;

	case GST_MESSAGE_STREAM_STATUS:
		if (player->verbose)
			player_handle_stream_status(player, message);
		break;

	case GST_MESSAGE_STATE_CHANGED:
		if (player->verbose)
			player_handle_state_changed(player, message);
		break;

	case GST_MESSAGE_TAG:
		if (player->verbose)
			player_handle_tag(player, message);
		break;

	default:
		g_print("   unhandled message: %s\n",
			gst_message_type_get_name(type));
		break;
	}

	return ret;
}

static int player_init_window(struct media_player *player)
{
	GdkWindowAttr attributes = {
		.width = 320,
		.height = 240,
		.wclass = GDK_INPUT_OUTPUT,
		.window_type = GDK_WINDOW_CHILD,
		.override_redirect = TRUE,
	};
//	GdkColormap *colormap;
	GdkRegion *region;


	player->window = gdk_window_new(NULL, &attributes, GDK_WA_NOREDIR);
	if (!player->window)
		return -ECONNREFUSED;

	gdk_window_set_decorations(player->window, 0);

	region = gdk_region_new();
	gdk_window_input_shape_combine_region(player->window, region, 0, 0);
	gdk_region_destroy(region);
/*
	colormap = gdk_drawable_get_colormap(player->window);
	if (!colormap) {
		colormap = gdk_colormap_new();
	}
*/
	player->xid = gdk_x11_drawable_get_xid(player->window);
	return 0;
}


static int player_init_gstreamer(struct media_player *player)
{
	GstElement *sink;

	char *argv[] = { "remote-control" };
	int argc = G_N_ELEMENTS(argv);

	gst_init(&argc, (char***)&argv);

	player->pipeline = gst_element_factory_make("playbin", NULL);
	if (!player->pipeline)
		return -ENOPROTOOPT;

	gst_bus_set_sync_handler(gst_pipeline_get_bus(
		GST_PIPELINE(player->pipeline)),
		(GstBusSyncHandler)player_bus_sync_handler, player);

	if (1) {
		GstElement *deinterlace;
		GstElement *videosink;
		GstPad *ghost;
		GstPad *pad;

		sink = gst_bin_new("deinterlaced videosink");
		deinterlace = gst_element_factory_make("deinterlace", NULL);
		/* mode=0:auto, mode=1:forced, mode=2:disabled */
		g_object_set(G_OBJECT(deinterlace), "mode", 0, NULL);
		g_object_set(G_OBJECT(deinterlace), "method", 4, NULL);
		videosink = gst_element_factory_make("autovideosink", NULL);

		gst_bin_add(GST_BIN(sink), deinterlace);
		gst_bin_add(GST_BIN(sink), videosink);
		gst_element_link(deinterlace, videosink);

		pad = gst_element_get_static_pad(deinterlace, "sink");
		ghost = gst_ghost_pad_new("sink", pad);
		gst_element_add_pad(sink, ghost);
	}

	g_object_set(G_OBJECT(player->pipeline), "video-sink", sink, NULL);

	return 0;
}


int media_player_create(struct media_player **playerp)
{
	struct media_player *player;
	int ret;

	if (!playerp)
		return -EINVAL;

	player = malloc(sizeof(*player));
	if (!player) {
		g_debug("out of memory\n");
		return -ENOMEM;
	}
	memset(player, 0, sizeof(*player));
	player->verbose = TRUE;

	ret = player_init_gstreamer(player);
	if (ret < 0)
		goto err;

	ret = player_init_window(player);
	if (ret < 0)
		goto err;

	g_debug("assigning player\n");
	*playerp = player;
	return 0;

err:
	g_debug("creating player failed %d\n", ret);
	return ret;
}

int media_player_free(struct media_player *player)
{
	if (!player)
		return -EINVAL;

	gst_element_set_state(player->pipeline, GST_STATE_NULL);
	gst_object_unref(player->pipeline);
	gst_deinit();

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
	gdk_window_clear(player->window);

	return 0;
}

int media_player_set_uri(struct media_player *player, const char *uri)
{
	GstStateChangeReturn ret;
	GstState state;

	if (!player || !uri)
		return -EINVAL;

	/* store player state for later */
	ret = gst_element_get_state(player->pipeline, &state, NULL,
		GST_CLOCK_TIME_NONE);
	if (ret == GST_STATE_CHANGE_FAILURE)
		state = GST_STATE_NULL;

	/* pause/stop player */
	ret = gst_element_set_state(player->pipeline, GST_STATE_NULL);
	if (ret == GST_STATE_CHANGE_FAILURE)
		return -ECANCELED;

	/* set new uri and restore priveous state */
	g_object_set(G_OBJECT(player->pipeline), "uri", uri, NULL);
	ret = gst_element_set_state(player->pipeline, state);
	if (ret == GST_STATE_CHANGE_FAILURE)
		return -ECANCELED;

	return 0;
}

int media_player_get_uri(struct media_player *player, char **urip)
{
	if (!player || !urip)
		return -EINVAL;


	return 0;
}

static int player_change_state(struct media_player *player, GstState state)
{
	GstStateChangeReturn ret;

	if (!player)
		return -EINVAL;

	ret = gst_element_set_state(player->pipeline, state);
	if (ret == GST_STATE_CHANGE_FAILURE)
		return -ECANCELED;

	return 0;
}

int media_player_play(struct media_player *player)
{
	int ret = player_change_state(player, GST_STATE_PLAYING);
	if (ret < 0)
		return ret;

	gdk_threads_enter();
	if (!gdk_window_is_visible(player->window))
		gdk_window_show(player->window);
	gdk_threads_leave();

	return ret;
}

int media_player_stop(struct media_player *player)
{
	int ret = player_change_state(player, GST_STATE_PAUSED);

	if (player) {
		gdk_threads_enter();
		gdk_window_hide(player->window);
		gdk_threads_leave();
	}

	return ret;
}

int media_player_get_state(struct media_player *player,
		enum media_player_state *statep)
{
	GstStateChangeReturn ret;
	GstState state;

	if (!player || !statep)
		return -EINVAL;

	ret = gst_element_get_state(player->pipeline, &state, NULL,
		GST_CLOCK_TIME_NONE);
	if (ret != GST_STATE_CHANGE_SUCCESS)
		return -EAGAIN;

	*statep = player_gst_state_2_media_state(state);
	return 0;
}
