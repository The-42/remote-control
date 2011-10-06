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

#include <inttypes.h>
#include <errno.h>

#include <gtk/gtk.h>
#include <gdk/gdkscreen.h>
#include <gdk/gdkwindow.h>
#include <gdk/gdkx.h>

#include <glib/gprintf.h>

#include <gst/gst.h>
#include <gst/interfaces/xoverlay.h>

#undef HAVE_SOFTWARE_DECODER
#define HAVE_SOFTWARE_DECODER 0

//#include <gst/playback/gstplay-enum.h> // not public
typedef enum {
	GST_PLAY_FLAG_VIDEO         = (1 << 0),
	GST_PLAY_FLAG_AUDIO         = (1 << 1),
	GST_PLAY_FLAG_TEXT          = (1 << 2),
	GST_PLAY_FLAG_VIS           = (1 << 3),
	GST_PLAY_FLAG_SOFT_VOLUME   = (1 << 4),
	GST_PLAY_FLAG_NATIVE_AUDIO  = (1 << 5),
	GST_PLAY_FLAG_NATIVE_VIDEO  = (1 << 6),
	GST_PLAY_FLAG_DOWNLOAD      = (1 << 7),
	GST_PLAY_FLAG_BUFFERING     = (1 << 8),
	GST_PLAY_FLAG_DEINTERLACE   = (1 << 9)
} GstPlayFlags;

/* defined in OMX gstomx-walvideosink */
#define NV_RENDER_TARGET_MIXER    0 /* prefered */
#define NV_RENDER_TARGET_EGLIMAGE 1
#define NV_RENDER_TARGET_OVERLAY  2 /* default */

#define NV_DEFAULT_RENDER_TARGET  NV_RENDER_TARGET_OVERLAY

#define DEFAULT_PIPELINE_FLAGS \
	(GST_PLAY_FLAG_VIDEO | GST_PLAY_FLAG_AUDIO | \
	GST_PLAY_FLAG_NATIVE_VIDEO | \
	GST_PLAY_FLAG_DOWNLOAD | GST_PLAY_FLAG_BUFFERING | \
	GST_PLAY_FLAG_DEINTERLACE)

#include "remote-control-stub.h"
#include "remote-control.h"

struct media_player {
	GstElement *pipeline;
	GdkWindow *window;  /* the window to use for output */
	XID xid;            /* the id of the x window */
	enum media_player_state state;

	gchar *uri;         /* the last used uri */
	int scale;          /* the scaler mode we have chosen */
};
#define SCALE_FULLSCREEN 0
#define SCALE_PREVIEW    1

static void dump_item(gpointer item, gpointer user)
{
	GstObject *obj = GST_OBJECT(item);
	if (obj) {
		g_printf("    Name: %s\n", GST_OBJECT_NAME(obj));
	}
}

static void dump_pipeline(GstBin *pipe)
{
	GstIterator *it;

	g_printf("> %s()\n", __func__);

	it = gst_bin_iterate_recurse(pipe);
	gst_iterator_foreach(it, dump_item, NULL);
	gst_iterator_free(it);
	g_printf("< %s()\n", __func__);
}

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

	gst_message_parse_error(message, &err, &debug);
	g_critical("Error: %s", err->message);

	g_error_free(err);
	g_free(debug);

	return 0;
}

static int player_warning_message(struct media_player *player, GstMessage *message)
{
	GError *error = NULL;
	gchar *debug = NULL;

	gst_message_parse_warning(message, &error, &debug);
	g_printf("WRN: %s\n", debug);

	g_error_free(error);
	g_free(debug);
	return 0;
}

#if 0
static int player_handle_stream_status(struct media_player *player, GstMessage *message)
{
	GstStreamStatusType type = GST_STREAM_STATUS_TYPE_CREATE;

	gst_message_parse_stream_status(message, &type, NULL);
	g_debug("   new stream status is: %d", type);

	return 0;
}

static int player_handle_state_changed(struct media_player *player, GstMessage *message)
{
	GstState old_state, new_state;

	gst_message_parse_state_changed(message, &old_state, &new_state, NULL);
	g_debug("   Element %s changed state from %s to %s.",
	        GST_OBJECT_NAME(message->src),
	        gst_element_state_get_name(old_state),
	        gst_element_state_get_name(new_state));

	return 0;
}
#endif

static void player_dump_tags(const GstTagList *list, const gchar *tag, gpointer user_data)
{
	char *value = NULL;
	switch (gst_tag_get_type(tag)) {
	case G_TYPE_STRING:
		if (gst_tag_list_get_string(list, tag, &value)) {
			g_debug("  TAG: %s = %s\n", tag, value);
			g_free(value);
		}
		break;
	default:
		break;
	}
}

static int player_handle_tag(struct media_player *player, GstMessage *message)
{
	GstTagList *tag_list = NULL;

	gst_message_parse_tag(message, &tag_list);
	gst_tag_list_foreach(tag_list, player_dump_tags, player);
	gst_tag_list_free(tag_list);

	return 0;
}

static void player_set_x_overlay(struct media_player *player)
{
//#if HAVE_SOFTWARE_DECODER
	GstElement *video_sink;
	g_printf("> %s()\n", __func__);

	video_sink = gst_bin_get_by_name(GST_BIN(player->pipeline), "gl-sink");
	if (!video_sink) {
		g_printf("< %s(): not found\n", __func__);
		return;
	}

	if (GDK_IS_WINDOW(player->window)) {
		g_printf("   gst_x_overlay_set_xwindow_id()....\n");
		gst_x_overlay_set_xwindow_id(GST_X_OVERLAY(video_sink),
		                             GDK_WINDOW_XWINDOW(player->window));
	} else {
		g_printf("   window not ready\n");
		gst_x_overlay_set_xwindow_id(GST_X_OVERLAY(video_sink), player->xid);
	}
	/* force redraw */
	gst_x_overlay_expose(GST_X_OVERLAY(video_sink));

	gst_object_unref(video_sink);
	g_printf("< %s()\n", __func__);
//#endif
}

static void player_set_x_window_id(struct media_player *player, const GValue *value)
{
#if HAVE_SOFTWARE_DECODER
	g_printf("> %s()\n", __func__);

	g_printf("    GValue=%p Type=%s\n", value, G_VALUE_TYPE_NAME(value));

	if (G_VALUE_HOLDS_POINTER(value)) {
		player->xid = (XID)(*((guintptr)g_value_get_pointer(value)));
	}

	if (G_VALUE_HOLDS_ULONG(value)) {
		player->xid = (XID)g_value_get_ulong(value);
	}

	g_printf("< %s()\n", __func__);
#endif
}

#if 0
static void parole_gst_set_x_overlay(struct media_player *player)
{
	GstElement *video_sink;

	g_object_get(G_OBJECT(player->pipeline), "video-sink", &video_sink, NULL);
	g_assert(video_sink != NULL);

	if (GDK_IS_WINDOW(player->window))
		gst_x_overlay_set_xwindow_id(GST_X_OVERLAY(video_sink),
			GDK_WINDOW_XWINDOW(player->window));

	gst_object_unref (video_sink);
}
#endif

static void player_element_message_sync(GstBus *bus, GstMessage *msg, struct media_player *player)
{
	const gchar* name;

	g_printf("> %s()\n", __func__);
	if (!msg->structure) {
		g_printf("< %s(): no structure\n", __func__);
		return;
	}
	name = gst_structure_get_name(msg->structure);
	g_printf("    name: %s\n", name);

	if (g_strcmp0(name, "prepare-xwindow-id") == 0) {
		player_set_x_overlay(player);
		g_printf("< %s(): prepare-xwindow-id\n", __func__);
		return;
	}

	if (g_strcmp0(name, "have-xwindow-id") == 0) {
		player_set_x_window_id(player,
			gst_structure_get_value(msg->structure, "have-xwindow-id"));
		g_printf("< %s(): have-xwindow-id\n", __func__);
		return;
	}

	g_printf("< %s()\n", __func__);
}

static gboolean player_gst_bus_event(GstBus *bus, GstMessage *msg, gpointer data)
{
	struct media_player *player = data;;

	switch (GST_MESSAGE_TYPE(msg)) {
	case GST_MESSAGE_EOS:
		g_printf("End of stream\n");
		player->state = MEDIA_PLAYER_STOPPED;
		break;

	case GST_MESSAGE_ERROR:
		player_error_message(player, msg);
		break;

	case GST_MESSAGE_BUFFERING: {
/*
		gint per = 0;
		gst_message_parse_buffering(msg, &per);
		g_printf("Buffering %d %%\n", per);
*/
		break;
	}
	case GST_MESSAGE_STATE_CHANGED: {
		GstState old, new, pending;
		gst_message_parse_state_changed(msg, &old, &new, &pending);
		if (GST_MESSAGE_SRC(msg) == GST_OBJECT(player->pipeline)) {
			g_printf("State changed: old=%d new=%d pending=%d\n", old, new, pending);
		}
		break;
	}

	case GST_MESSAGE_TAG:
		player_handle_tag(player, msg);
		break;
	case GST_MESSAGE_APPLICATION:
		break;
	case GST_MESSAGE_DURATION:
		break;
	case GST_MESSAGE_ELEMENT:
		player_element_message_sync(bus, msg, player);
		//player_error_message(player, msg);
		break;
	case GST_MESSAGE_WARNING:
		player_warning_message(player, msg);
		break;
	case GST_MESSAGE_INFO:
		break;
	case GST_MESSAGE_STATE_DIRTY:
		g_printf("Stream is dirty\n");
		break;
	case GST_MESSAGE_STEP_DONE:
		break;
	case GST_MESSAGE_CLOCK_PROVIDE:
		break;
	case GST_MESSAGE_CLOCK_LOST:
		break;
	case GST_MESSAGE_NEW_CLOCK:
		break;
	case GST_MESSAGE_STRUCTURE_CHANGE:
		break;
	case GST_MESSAGE_STREAM_STATUS:
		break;
	case GST_MESSAGE_SEGMENT_START:
		break;
	case GST_MESSAGE_LATENCY:
		break;
	case GST_MESSAGE_ASYNC_START:
		break;
	case GST_MESSAGE_ASYNC_DONE:
		break;
	default:
		break;
	}
	return TRUE;
}

static GstBusSyncReply player_gst_bus_sync_handler(GstBus *bus, GstMessage *message,
                gpointer user_data)
{
	struct media_player *player = user_data;
	GstMessageType type = GST_MESSAGE_TYPE(message);
	GstBusSyncReply ret = GST_BUS_PASS;

	switch (type) {
	case GST_MESSAGE_ELEMENT:
#if HAVE_SOFTWARE_DECODER
		/*
		 * TODO: find another way to get the window id
		 */
		g_debug("** element: %s", gst_structure_get_name(message->structure));
		if (!gst_structure_has_name(message->structure, "prepare-xwindow-id") &&
		    !gst_structure_has_name(message->structure, "have-xwindow-id")) {
			g_debug("** no xwindow found");
			break;
		}
		g_debug("**  showing window...");
		gdk_threads_enter();
		g_debug("    looked");
		gdk_window_show(player->window);
		g_debug("    show");
		gdk_threads_leave();
		g_debug("    unlooked");

		g_debug("**  try to get XID...");
		if (player && player->xid != 0) {
			GstXOverlay *overlay = GST_X_OVERLAY(GST_MESSAGE_SRC(message));
//			g_object_set(overlay, "force-aspect-ratio", TRUE, NULL);
			player->xid = gdk_x11_drawable_get_xid(player->window);
			g_debug("**  XID is: %lu", player->xid);
			gst_x_overlay_set_window_handle(overlay, player->xid);
		} else {
			g_debug("**  %p:%lu: no window XID yet!", player, player->xid);
		}

		gst_message_unref(message);
		ret = GST_BUS_DROP;
#endif
		break;

	case GST_MESSAGE_BUFFERING:
		/* silently ignore */
		break;

	case GST_MESSAGE_WARNING:
		/* silently ignore */
		break;

	case GST_MESSAGE_ERROR:
		ret = player_error_message(player, message);
	case GST_MESSAGE_EOS:
#if HAVE_SOFTWARE_DECODER
		gdk_threads_enter();
		gdk_window_hide(player->window);
		gdk_threads_leave();
#endif
		g_debug("**  hiding window %s", ret == GST_BUS_PASS ? "" : "due error");
		break;

	case GST_MESSAGE_STREAM_STATUS:
//		player_handle_stream_status(player, message);
		break;

	case GST_MESSAGE_STATE_CHANGED:
//		player_handle_state_changed(player, message);
		break;

	case GST_MESSAGE_TAG:
		player_handle_tag(player, message);
		break;

	default:
		g_printf("   ignore: %s\n",
		         gst_message_type_get_name(type));
		break;
	}

	return ret;
}

static int player_init_window(struct media_player *player)
{
	GdkWindowAttr attributes = {
//		.width = 320, .height = 240,
		.x = 34, .y = 55, .width = 765, .height = 422,
		.wclass = GDK_INPUT_OUTPUT,
		.window_type = GDK_WINDOW_CHILD,
		.override_redirect = TRUE,
	};
	GdkRegion *region;

	player->window = gdk_window_new(NULL, &attributes, GDK_WA_NOREDIR);
	if (!player->window)
		return -ECONNREFUSED;

	gdk_window_set_decorations(player->window, 0);

	region = gdk_region_new();
	gdk_window_input_shape_combine_region(player->window, region, 0, 0);
	gdk_region_destroy(region);

	player->xid = gdk_x11_drawable_get_xid(player->window);
	return 0;
}

static int player_destroy_pipeline(struct media_player *player)
{
	if (player->uri) {
		g_free(player->uri);
		player->uri = NULL;
	}
	if (player->pipeline) {
		gst_element_set_state(player->pipeline, GST_STATE_NULL);
		gst_object_unref(player->pipeline);
		player->pipeline = NULL;
	}
	return 0;
}

static int player_create_software_pipeline(struct media_player *player, const gchar* uri)
{
#if HAVE_SOFTWARE_DECODER
#define PIPELINE_INPUT_UDP  "udpsrc name=source uri=%s "
#define PIPELINE_INPUT_FILE "filesrc name=source location=%s"
#define PIPELINE_INPUT_HTTP "souphttpsrc name=source location=%s"
#define PIPELINE_INPUT_DUMMY "videotestsrc name=source ! videoscale ! glimagesink"

#define PIPELINE_FFMPEG \
	" ! queue ! mpegtsdemux name=demux " \
		"demux. ! queue ! mpegvideoparse ! ffdec_mpeg2video max-threads=2 ! " \
			"ffdeinterlace ! glimagesink " \
		"demux. ! queue !  nv_omx_mp2dec ! alsasink device=hw:1,0"

#define PIPELINE_NVIDIA \
	" ! queue name=input-queue ! mpegtsdemux name=demux " \
		"demux. ! queue2 name=video-queue ! mpegvideoparse name=mpeg-parser ! " \
			"nv_omx_mpeg2dec name=video-decoder ! " \
				"nv_omx_overlaysink name=gl-sink " \
		"demux. ! queue2 name=audio-queue ! nv_omx_mp2dec name=audio-decoder ! " \
			"alsasink name=audio-out device=hw:1,0"

#define PIPELINE PIPELINE_NVIDIA

	GstElement *out = NULL;
	GError *error = NULL;
	GstBus *bus;
	gchar *pipe;
	int type = 0;
	int ret = -EINVAL;

	if (player->pipeline)
		player_destroy_pipeline(player);

	if (g_str_has_prefix(uri, "udp://")) {
		pipe = g_strdup_printf(PIPELINE_INPUT_UDP PIPELINE, uri);
		type = 0;
	} else if (g_str_has_prefix(uri, "http://")) {
		pipe = g_strdup_printf(PIPELINE_INPUT_HTTP PIPELINE, uri);
		type = 1;
	} else if (g_str_has_prefix(uri, "file://")) {
		pipe = g_strdup_printf(PIPELINE_INPUT_FILE PIPELINE, uri);
		type = 2;
	} else /* create dummy */ {
		pipe = g_strdup(PIPELINE_INPUT_DUMMY);
		type = 3;
	}

	player->pipeline = gst_parse_launch_full(pipe, NULL, GST_PARSE_FLAG_FATAL_ERRORS, &error);
	if (!player->pipeline) {
		g_warning("no pipe: %s\n", error->message);
		goto cleanup;
	}

	out = gst_bin_get_by_name(GST_BIN(player->pipeline), "gl-sink");
	if (out) {
		g_object_set(G_OBJECT(out), "xwindow", player->xid, NULL);
		g_object_set(G_OBJECT(out), "rendertarget", NV_RENDER_TARGET_MIXER, NULL);
	}
	else
		g_warning("element video-out not found");

	/* setup message handling */
	bus = gst_pipeline_get_bus(GST_PIPELINE(player->pipeline));
	if (bus) {
		gst_bus_add_watch(bus, (GstBusFunc)player_gst_bus_event, player);
		gst_bus_set_sync_handler(bus,
			(GstBusSyncHandler)player_gst_bus_sync_handler, player);
		gst_object_unref(GST_OBJECT(bus));
		ret = 0;
	}

cleanup:
	if (error)
		g_error_free(error);
	g_free(pipe);
	return ret;
#else
	return 0;
#endif
}

static int player_create_nvidia_pipeline(struct media_player *player, const gchar* uri)
{
	GstElement *audiosink = NULL;
	GstElement *videosink = NULL;
	GstElement *pipeline = NULL;
	GstPlayFlags flags = 0;
	GstBus *bus = NULL;

	g_debug("> %s(player=%p, uri=%p)", __func__, player, uri);
	if (!player || !uri)
		return -EINVAL;

	g_debug("   create playbin2...");
	pipeline = gst_element_factory_make("playbin2","pipeline");
	if (!pipeline) {
		g_critical("failed to create pipeline");
		goto cleanup;
	}
	/* make sure we have audio on hdmi */
	g_debug("   create alsasink...");
	audiosink = gst_element_factory_make("alsasink", "audio-output");
	if (!audiosink) {
		g_critical("failed to create audiosink");
		goto cleanup;
	}
	g_object_set(G_OBJECT(audiosink), "device", "hdmi", NULL);
	g_object_set(G_OBJECT(pipeline), "audio-sink", audiosink, NULL);
	/* make sure we use hartware accellerated output */
	g_debug("   create nv_gl_videosink...");
	videosink = gst_element_factory_make("nv_gl_videosink", "video-output");
	if (!videosink) {
		g_critical("failed to create videosink");
		goto cleanup;
	}
	if (player->scale == SCALE_PREVIEW)
		g_object_set(G_OBJECT(videosink), "rendertarget", NV_RENDER_TARGET_MIXER, NULL);
	else
		g_object_set(G_OBJECT(videosink), "rendertarget", NV_RENDER_TARGET_OVERLAY, NULL);
	g_object_set(G_OBJECT(videosink), "force-aspect-ratio", TRUE, NULL);
	g_object_set(G_OBJECT(pipeline), "video-sink", videosink, NULL);
	/* make sure we can playback using playbin2 */
	g_debug("   configure playbin2...");
	g_object_get(G_OBJECT(pipeline), "flags", &flags, NULL);
	flags |= GST_PLAY_FLAG_BUFFERING | GST_PLAY_FLAG_NATIVE_VIDEO;
//	flags = 0x1F3; /* video,audio,buffer,volume */
	g_object_set(G_OBJECT(pipeline), "flags", flags, NULL);
	g_object_set(G_OBJECT(pipeline), "uri", uri, NULL);

	/* register callback so we get access to the xwindow-id and other
	 * events like EOS or  */
	g_debug("   register bus event handler...");
	bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
	if (bus) {
		gst_bus_add_watch(bus, (GstBusFunc)player_gst_bus_event, player);
		gst_bus_set_sync_handler(bus,
			(GstBusSyncHandler)player_gst_bus_sync_handler, player);
		gst_object_unref(GST_OBJECT(bus));
	}

	{
		g_debug("   looking for overlay");
		GstElement *overlay = gst_bin_get_by_name(GST_BIN(pipeline), "gl-sink");
		if (!overlay)
			overlay = gst_bin_get_by_name(GST_BIN(pipeline), "overlay-sink");

		if (overlay) {
			g_debug("   setting xwindow id");
			g_object_set(G_OBJECT(overlay), "xwindow", player->xid, NULL);
			gst_object_unref(GST_OBJECT(overlay));
		}
	}

	player->pipeline = pipeline;
	g_debug("< %s(): pipeline ready", __func__);
	return 0;

cleanup:
	if (audiosink)
		gst_object_unref(GST_OBJECT(audiosink));
	if (videosink)
		gst_object_unref(GST_OBJECT(videosink));
	if (pipeline)
		gst_object_unref(GST_OBJECT(pipeline));
	g_debug("< %s(): pipeline broken", __func__);
	return -1;
}

static int player_create_pipeline(struct media_player *player, const gchar* uri)
{
	int ret;
	g_debug("> %s(player=%p, uri=%s)", __func__, player, uri);

	if (HAVE_SOFTWARE_DECODER) {
		g_debug("   building software pipe...");
		ret = player_create_software_pipeline(player, uri);
	} else {
		g_debug("   building nvidia pipe...");
		ret = player_create_nvidia_pipeline(player, uri);
	}

	if (ret < 0 && player->pipeline) {
		if (player->uri)
			g_free(player->uri);
		player->uri = g_strdup(uri);
	}

	dump_pipeline(GST_BIN(player->pipeline));

	g_debug("< %s()", __func__);
	return ret;
}

static int player_init_gstreamer(struct media_player *player)
{
	GError *err = NULL;
	/* FIXME: we need the real arguments, otherwise we can not pass things
	 *        like GST_DEBUG=2 ??? */
	char *argv[] = { "remote-control", NULL };
	int argc = 1;

	g_debug("> %s(player=%p)", __func__, player);

	/* we need to fake the args, because we have no access from here */
	g_debug("   initialize gstreamer...");
	if (!gst_init_check(&argc, (char***)&argv, &err)) {
		g_error_free(err);
		g_critical("failed to initialize gstreamer");
		return -ENOSYS;
	}

	g_debug("< %s()", __func__);
	return 0;
}

static int player_change_state(struct media_player *player, GstState state)
{
	GstStateChangeReturn ret;

	if (!player)
		return -EINVAL;

	ret = gst_element_set_state(player->pipeline, state);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		g_critical("failed to set player state");
		return -ECANCELED;
	}

	return 0;
}

static int player_set_uri(struct media_player *player, const char *uri)
{
	g_debug("> player_set_uri(uri=%s)", uri);

	if (!g_str_has_prefix(uri, "udp://") &&
	    !g_str_has_prefix(uri, "file://") &&
	    !g_str_has_prefix(uri, "http://"))
	{
		g_warning("unsupported uri");
		return -EINVAL;
	}

	/* because url can be valid, but we do not know the content, we can
	 * not be sure that the chain can handle the new url we destroy the
	 * old and create a new. this is the safest way */
	g_debug("   destroying old pipeline...");
	player_destroy_pipeline(player);
	g_debug("   creating new pipeline...");
	if (player_create_pipeline(player, (const gchar*)uri) < 0) {
		g_critical("failed to create pipeline");
		return -ENOSYS;
	}

	g_debug("< player_set_uri()");
	return 0;
}

static int player_get_uri(struct media_player *player, char **urip)
{
	if (!player->pipeline)
		return -EINVAL;

	*urip = strdup(player->uri);
	return urip == NULL ? -ENOMEM : 0;
}


/**
 * HERE comes the part for remote-control
 */
int media_player_create(struct media_player **playerp)
{
	struct media_player *player = NULL;
	int ret;

	g_debug("> %s(playerp=%p)", __func__, playerp);
	if (!playerp)
		return -EINVAL;

	g_debug("   allocation memory...");
	player = g_new0(struct media_player, 1);
	if (player == NULL) {
		g_warning("out of memory\n");
		return -ENOMEM;
	}

	g_debug("   setting up gstreamer...");
	ret = player_init_gstreamer(NULL);
	if (ret < 0) {
		g_warning("failed to initialize gstreamer %d", ret);
		goto err;
	}

	g_debug("   creating pipeline...");
	ret = player_create_pipeline(player, "none");
	if (ret < 0 || player->pipeline == NULL) {
		g_warning("failed to setup pipeline");
		goto err;
	}

	g_debug("   preparing window...");
	ret = player_init_window(player);
	if (ret < 0) {
		g_warning("failed to create window %d", ret);
		goto err;
	}

	g_debug("   assigning player...");
	*playerp = player;
	return 0;

err:
	g_critical("creating player failed %d", ret);
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

	g_free(player);
	return 0;
}

int media_player_set_uri(struct media_player *player, const char *uri)
{
	int err;

	if (!player || !uri)
		return -EINVAL;

	err = player_set_uri(player, uri);
	if (err < 0)
		return err;

	return 0;
}

int media_player_get_uri(struct media_player *player, char **urip)
{
	int err;

	if (!player || !urip)
		return -EINVAL;

	err = player_get_uri(player, urip);
	if (err < 0)
		return err;

	return 0;
}

int media_player_set_output_window(struct media_player *player,
                                   unsigned int x, unsigned int y,
                                   unsigned int width,
                                   unsigned int height)
{
	if (!player)
		return -EINVAL;

	g_debug("> %s(player=%p, x=%d, y=%d, width=%d, height=%d)",
		__func__, player, x, y, width, height);

#if 1/*HAVE_SOFTWARE_DECODER*/
	gdk_window_move_resize(player->window, x, y, width, height);
	gdk_window_clear(player->window);
//#else
	if ((x < 10) && (y < 10) && (width > 1280) && (height > 720))
		player->scale = SCALE_FULLSCREEN;
	else
		player->scale = SCALE_PREVIEW;
#else
	/* this will destroy the pipeline and create a new one */
	if (player->uri) {
		gchar *uri = g_strdup(player->uri);
		media_player_set_uri(player, uri);
		g_free(uri);
	}
#endif
	g_debug("< %s()", __func__);
	return 0;
}

int media_player_play(struct media_player *player)
{
	int ret = player_change_state(player, GST_STATE_PLAYING);
	if (ret < 0)
		return ret;

#if HAVE_SOFTWARE_DECODER
	gdk_threads_enter();
	if (!gdk_window_is_visible(player->window))
		gdk_window_show(player->window);
	gdk_threads_leave();
#else
//	ret = player_create_pipeline(player, player->uri);
#endif
	return ret;
}

int media_player_stop(struct media_player *player)
{
#if HAVE_SOFTWARE_DECODER
	int ret = player_change_state(player, GST_STATE_PAUSED);

	if (player) {
		gdk_threads_enter();
		gdk_window_hide(player->window);
		gdk_threads_leave();
	}

	return ret;
#else
	int ret;
	gchar *uri = g_strdup(player->uri);

	ret = player_destroy_pipeline(player);
	player->uri = uri;

	return ret;
#endif
}

int media_player_get_state(struct media_player *player,
                           enum media_player_state *statep)
{
	GstStateChangeReturn ret;
	GstState state = MEDIA_PLAYER_STOPPED;

	if (!player || !statep)
		return -EINVAL;

	g_debug("> %s(player=%p, statep=%p)", __func__, player, statep);
	if (player->pipeline) {
		/* This call can block under some conditions, so it is better to use
		 * a timeout here. */
		ret = gst_element_get_state(player->pipeline, &state, NULL, GST_SECOND);
		if (ret != GST_STATE_CHANGE_SUCCESS) {
			if (ret != GST_STATE_CHANGE_NO_PREROLL) {
				g_critical("failed to get player state: %d", ret);
				return -ETIMEDOUT;
			}
		}
	}

	*statep = player_gst_state_2_media_state(state);
	g_debug("< %s(): state=%d", __func__, *statep);
	return 0;
}
