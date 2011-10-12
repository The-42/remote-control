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

#include <X11/extensions/Xrandr.h>

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

#define NV_DISPLAY_TYPE_DEFAULT   0
#define NV_DISPLAY_TYPE_CRT       1
#define NV_DISPLAY_TYPE_HDMI      2
#define NV_DISPLAY_TYPE_LVDS      3

#define DEFAULT_PIPELINE_FLAGS \
	(GST_PLAY_FLAG_VIDEO | GST_PLAY_FLAG_AUDIO | \
	GST_PLAY_FLAG_NATIVE_VIDEO | \
	GST_PLAY_FLAG_DOWNLOAD | GST_PLAY_FLAG_BUFFERING | \
	GST_PLAY_FLAG_DEINTERLACE)

#include "remote-control-stub.h"
#include "remote-control.h"

#define SCALE_PREVIEW    0
#define SCALE_FULLSCREEN 1

struct media_player {
	GstElement *pipeline;
	GdkWindow *window;   /* the window to use for output */
	XID xid;             /* the id of the x window */
	enum media_player_state state;

	gchar *uri;         /* the last used uri */
	int scale;          /* the scaler mode we have chosen */
	int displaytype;
};

static void player_dump(struct media_player *player)
{
	g_printf(" struct media_player:... %p\n", player);
	if (!player)
		return;

	g_printf("   pipeline:............ %p\n", player->pipeline);
	g_printf("   window:.............. %p\n", player->window);
	g_printf("   xid:................. %d\n", (int)player->xid);
	g_printf("   state:............... %d\n", player->state);
	g_printf("   uri:................. [%s]\n", player->uri);
	g_printf("   scale:............... %d\n", player->scale);
	g_printf("   displaytype:......... %d\n", player->displaytype);
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

static void handle_message_state_change(struct media_player *player, GstMessage *message)
{
	GstState pending = GST_STATE_VOID_PENDING;
	GstState old = GST_STATE_VOID_PENDING;
	GstState new = GST_STATE_VOID_PENDING;

	gst_message_parse_state_changed(message, &old, &new, &pending);
	if (GST_MESSAGE_SRC(message) == GST_OBJECT(player->pipeline)) {
		g_printf("   Element %s changed state from %s to %s (pending=%s)\n",
			GST_OBJECT_NAME(message->src),
			gst_element_state_get_name(old),
			gst_element_state_get_name(new),
			gst_element_state_get_name(pending));
	}
}

static int handle_message_error(struct media_player *player, GstMessage *message)
{
	GError *err;
	gchar *debug;

	gst_message_parse_error(message, &err, &debug);
	g_printf("ERROR: %s", err->message);

	g_error_free(err);
	g_free(debug);

	return 0;
}

static int handle_message_warning(struct media_player *player, GstMessage *message)
{
	GError *error = NULL;
	gchar *debug = NULL;

	gst_message_parse_warning(message, &error, &debug);
	g_printf("WRN: %s\n", debug);

	g_error_free(error);
	g_free(debug);
	return 0;
}

static void handle_message_info(struct media_player *player, GstMessage *message)
{
	GError *error = NULL;
	gchar *debug = NULL;

	gst_message_parse_warning(message, &error, &debug);
	g_printf("INFO: %s\n", debug);

	g_error_free(error);
	g_free(debug);
}

static void player_set_x_overlay(struct media_player *player)
{
	GstElement *video_sink;

	g_debug(" > %s()", __func__);
	video_sink = gst_bin_get_by_name(GST_BIN(player->pipeline), "gl-sink");
	if (!video_sink) {
		g_warning("< %s(): not found", __func__);
		return;
	}

	if (GDK_IS_WINDOW(player->window)) {
		g_debug("   gst_x_overlay_set_xwindow_id()....");
		gst_x_overlay_set_xwindow_id(GST_X_OVERLAY(video_sink),
		                             GDK_WINDOW_XWINDOW(player->window));
	} else {
		g_debug("   window not ready");
		gst_x_overlay_set_xwindow_id(GST_X_OVERLAY(video_sink), player->xid);
	}

	/* force redraw */
	gst_x_overlay_expose(GST_X_OVERLAY(video_sink));

	gst_object_unref(video_sink);
	g_debug(" < %s()", __func__);
}

static void player_set_x_window_id(struct media_player *player, const GValue *value)
{
#if HAVE_SOFTWARE_DECODER
	g_debug(" > %s()", __func__);

	g_printf("    GValue=%p Type=%s", value, G_VALUE_TYPE_NAME(value));

	if (G_VALUE_HOLDS_POINTER(value)) {
		player->xid = (XID)(*((guintptr)g_value_get_pointer(value)));
	}

	if (G_VALUE_HOLDS_ULONG(value)) {
		player->xid = (XID)g_value_get_ulong(value);
	}

	g_printf(" < %s()", __func__);
#endif
}

static void player_element_message_sync(GstBus *bus, GstMessage *msg, struct media_player *player)
{
	const gchar* name;

	g_debug("> %s()", __func__);
	if (!msg->structure) {
		g_debug("< %s(): no structure", __func__);
		return;
	}

	name = gst_structure_get_name(msg->structure);
	g_debug("    name: %s", name);

	if (g_strcmp0(name, "prepare-xwindow-id") == 0) {
		player_set_x_overlay(player);
		g_debug("< %s(): prepare-xwindow-id", __func__);
		return;
	}

	if (g_strcmp0(name, "have-xwindow-id") == 0) {
		player_set_x_window_id(player,
			gst_structure_get_value(msg->structure, "have-xwindow-id"));
		g_debug("< %s(): have-xwindow-id", __func__);
		return;
	}

	g_debug("< %s()", __func__);
}

static gboolean player_gst_bus_event(GstBus *bus, GstMessage *msg, gpointer data)
{
	struct media_player *player = data;

	switch (GST_MESSAGE_TYPE(msg)) {
	case GST_MESSAGE_EOS:
		g_debug("End of stream");
		player->state = MEDIA_PLAYER_STOPPED;
		break;
	case GST_MESSAGE_ERROR:
		handle_message_error(player, msg);
		break;
	case GST_MESSAGE_WARNING:
		handle_message_warning(player, msg);
		break;
	case GST_MESSAGE_INFO:
		handle_message_info(player, msg);
		break;
	case GST_MESSAGE_STATE_CHANGED:
		handle_message_state_change(player, msg);
		break;
	case GST_MESSAGE_STATE_DIRTY:
		g_warning("Stream is dirty\n");
		break;
	case GST_MESSAGE_BUFFERING:
		break;
	case GST_MESSAGE_TAG:
		break;
	case GST_MESSAGE_ELEMENT:
		player_element_message_sync(bus, msg, player);
		break;
	case GST_MESSAGE_APPLICATION:
		break;
	case GST_MESSAGE_DURATION:
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
		ret = handle_message_error(player, message);
	case GST_MESSAGE_EOS:
#if HAVE_SOFTWARE_DECODER
		gdk_threads_enter();
		gdk_window_hide(player->window);
		gdk_threads_leave();
#endif
		g_debug("**  hiding window %s", ret == GST_BUS_PASS ? "" : "due error");
		break;

	case GST_MESSAGE_STREAM_STATUS:
		break;

	case GST_MESSAGE_STATE_CHANGED:
		break;

	case GST_MESSAGE_TAG:
		break;

	default:
		g_printf("   ignore: %s\n",
		         gst_message_type_get_name(type));
		break;
	}

	return ret;
}

static int player_window_move(struct media_player *player,
                              int x, int y, int width, int height)
{
	GstElement *video;

	g_debug("  > %s(player=%p, x=%d, y=%d, width=%d, height=%d)",
		 __func__, player, x, y, width, height);

	video = gst_bin_get_by_name(GST_BIN(player->pipeline), "video-out");
	if (!video) {
		g_warning("   no element video-out found");
		g_debug("  < %s(): no data", __func__);
		return -ENODATA;
	}

	/* the height must be the last parameter we set, this is because the
	 * plugin only updates x,y,w when h is set */
	g_object_set(G_OBJECT(video), "output-pos-x", x, "output-pos-y", y,
		"output-size-width", width, "output-size-height", height,
		NULL);
	gst_object_unref(video);

	g_debug("  < %s()", __func__);
	return 0;
}

static int player_window_update(struct media_player *player)
{
	gint x, y, width, height;
	int ret;

	g_debug("  > %s()", __func__);

	if (!player || !player->window) {
		g_debug("  < %s(): inval", __func__);
		return -EINVAL;
	}

	gdk_window_get_position(GDK_WINDOW(player->window), &x, &y);
	gdk_drawable_get_size(GDK_DRAWABLE(player->window), &width, &height);

	ret = player_window_move(player, x, y, width, height);

	g_debug("  < %s(): %d", __func__, ret);
	return ret;
}

static int player_window_init(struct media_player *player)
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

#define PIPELINE_AUTO \
	" ! decodebin2 flags=0x57 "\
		"video-sink=\"autovideosink name=video-out\" " \
		"audio-sink=\"autoaudiosink name=audio-out\" "

#define PIPELINE_FFMPEG \
	" ! queue ! mpegtsdemux name=demux " \
		"demux. ! queue ! mpegvideoparse ! ffdec_mpeg2video max-threads=2 ! " \
			"ffdeinterlace ! autovideosink name=video-out " \
		"demux. ! queue ! mpegaudioparse ! ffdec_mp3 ! " \
			"autoaudiosink name=audio-out "

#define PIPELINE PIPELINE_FFMPEG

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

static int player_create_manual_nvidia_pipeline(struct media_player *player, const gchar* uri)
{
#define PIPELINE_INPUT_UDP  "udpsrc do-timestamp=1 name=source uri=%s "
#define PIPELINE_INPUT_FILE "filesrc name=source location=%s"
#define PIPELINE_INPUT_HTTP "souphttpsrc name=source location=%s"
#define PIPELINE_INPUT_DUMMY "videotestsrc name=source ! videoscale ! glimagesink"

#define PIPELINE_MANUAL \
	" ! queue max-size-buffers=512 leaky=1 name=input-queue ! mpegtsdemux name=demux " \
		"demux. ! queue2 name=audio-queue ! mpegaudioparse name=audio-parser ! " \
			"nv_omx_mp2dec name=audio-decoder ! " \
				"audioconvert ! audioresample ! volume name=volume ! " \
				"alsasink name=audio-out device=hw:%d,0 " \
		"demux. ! queue2 name=video-queue ! mpegvideoparse name=video-parser ! " \
			"nv_omx_mpeg2dec name=video-decoder ! " \
				"nv_gl_videosink name=video-out deint=3 " \
					"rendertarget=%d displaytype=%d "

	GError *error = NULL;
	GstBus *bus;
	gchar *pipe;
	int ret = -EINVAL;
	int rt, dt, ad;

	if (player->pipeline)
		player_destroy_pipeline(player);

	rt = player->scale == SCALE_PREVIEW ? NV_RENDER_TARGET_MIXER : NV_RENDER_TARGET_OVERLAY;
	dt = player->displaytype;
	ad = player->displaytype == NV_DISPLAY_TYPE_HDMI ? 1 : 0;

	if (uri == NULL) {
		pipe = g_strdup(PIPELINE_INPUT_DUMMY);
	} else if (g_str_has_prefix(uri, "udp://")) {
		pipe = g_strdup_printf(PIPELINE_INPUT_UDP PIPELINE_MANUAL, uri, ad, rt, dt);
	} else if (g_str_has_prefix(uri, "http://")) {
		pipe = g_strdup_printf(PIPELINE_INPUT_HTTP PIPELINE_MANUAL, uri, ad, rt, dt);
	} else if (g_str_has_prefix(uri, "file://")) {
		pipe = g_strdup_printf(PIPELINE_INPUT_FILE PIPELINE_MANUAL, uri, ad, rt, dt);
	} else {
		pipe = g_strdup(PIPELINE_INPUT_DUMMY);
	}

	g_debug(pipe);

	player->pipeline = gst_parse_launch_full(pipe, NULL, GST_PARSE_FLAG_FATAL_ERRORS, &error);
	if (!player->pipeline) {
		g_warning("no pipe: %s\n", error->message);
		goto cleanup;
	}

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
}

static int player_create_automatic_nvidia_pipeline(struct media_player *player, const gchar* uri)
{
	GstElement *audiosink = NULL;
	GstElement *videosink = NULL;
	GstElement *pipeline = NULL;
	GstPlayFlags flags = 0;
	GstBus *bus = NULL;

	g_debug(" > %s(player=%p, uri=%p)", __func__, player, uri);
	if (!player || !uri)
		return -EINVAL;

	if (player->pipeline)
		player_destroy_pipeline(player);

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
	/* FIXME: the audio-device must be configurable and/or auto detected */
	if (player->displaytype == NV_DISPLAY_TYPE_HDMI)
		g_object_set(G_OBJECT(audiosink), "device", "hw:1,0", NULL);
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

	g_object_set(G_OBJECT(videosink), "displaytype", player->displaytype, NULL);
	g_object_set(G_OBJECT(videosink), "force-aspect-ratio", TRUE, NULL);
	g_object_set(G_OBJECT(pipeline), "video-sink", videosink, NULL);
	/* make sure we can playback using playbin2 */
	g_debug("   configure playbin2...");
	g_object_get(G_OBJECT(pipeline), "flags", &flags, NULL);
	flags |= GST_PLAY_FLAG_NATIVE_VIDEO;
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

	player->pipeline = pipeline;
	g_debug(" < %s(): pipeline ready", __func__);
	return 0;

cleanup:
	if (audiosink)
		gst_object_unref(GST_OBJECT(audiosink));
	if (videosink)
		gst_object_unref(GST_OBJECT(videosink));
	if (pipeline)
		gst_object_unref(GST_OBJECT(pipeline));
	g_debug(" < %s(): pipeline broken", __func__);
	return -1;
}

static int player_create_pipeline(struct media_player *player, const gchar* uri)
{
	int ret;
	g_debug(" > %s(player=%p, uri=%s)", __func__, player, uri);

	if (HAVE_SOFTWARE_DECODER) {
		g_debug("   building software pipe...");
		ret = player_create_software_pipeline(player, uri);
	} else if (0) {
		g_debug("   building automatic nvidia pipe...");
		ret = player_create_automatic_nvidia_pipeline(player, uri);
	} else {
		g_debug("   building manual nvidia pipe...");
		ret = player_create_manual_nvidia_pipeline(player, uri);
	}

	if (ret < 0 && player->pipeline) {
		if (player->uri) {
			g_free(player->uri);
			player->uri = NULL;
		}
	}
	if (!player->uri)
		player->uri = g_strdup(uri);
	g_debug("   dumping player");
	player_dump(player);
	g_debug(" < %s(): %d", __func__, ret);
	return ret;
}

static int player_init_gstreamer(struct media_player *player)
{
	GError *err = NULL;
	/* FIXME: we need the real arguments, otherwise we can not pass things
	 *        like GST_DEBUG=2 ??? */
	char *argv[] = { "remote-control", NULL };
	int argc = 1;

	g_debug(" > %s(player=%p)", __func__, player);

	/* we need to fake the args, because we have no access from here */
	g_debug("   initialize gstreamer...");
	if (!gst_init_check(&argc, (char***)&argv, &err)) {
		g_error_free(err);
		g_critical("failed to initialize gstreamer");
		return -ENOSYS;
	}

	g_debug(" < %s()", __func__);
	return 0;
}

	/* only valid for tegra */
static int tegra_display_name_to_type(const gchar *name)
{
	struct connection_map {
		const gchar *name;
		int type;
	};

	static const struct connection_map MAP[] = {
		{ "CRT",    NV_DISPLAY_TYPE_CRT  },
		{ "HDMI",   NV_DISPLAY_TYPE_HDMI },
		{ "TFTLCD", NV_DISPLAY_TYPE_LVDS }
	};
	int index;

	for (index = 0; index < G_N_ELEMENTS(MAP); index++) {
		if (g_strcmp0(name, MAP[index].name) == 0)
			return MAP[index].type;
	}

	return NV_DISPLAY_TYPE_DEFAULT;
}

static int player_find_display_type(struct media_player *player)
{
	XRRScreenResources *screen_res;
	XRROutputInfo *output_info;
	GdkDisplay *display;
	GdkScreen *screen;
	GdkWindow *window;
	Display *xdisplay;
	XID rootwindow;
	int event_base = 0;
	int error_base = 0;
	int i;

	display = gdk_display_get_default();
	if (!display) {
		g_warning("no display");
		return -1;
	}
	screen = gdk_display_get_default_screen(display);
	if (!screen) {
		g_warning("no screen");
		return -1;
	}
	window = gdk_screen_get_root_window(screen);
	if (!window) {
		g_warning("no window");
		return -1;
	}

	xdisplay = gdk_x11_display_get_xdisplay(display);
	if (!xdisplay) {
		g_warning("no xdisplay");
		return -1;
	}
	rootwindow = gdk_x11_drawable_get_xid(GDK_DRAWABLE(window));

	if (!XRRQueryExtension(xdisplay, &event_base, &error_base)) {
		g_warning("randr not available: %d %d", event_base, error_base);
		return -ENOSYS;
	}

	screen_res = XRRGetScreenResources(xdisplay, rootwindow);
	if (!screen_res) {
		g_warning("no screen resource");
		return -ENOMEM;
	}

	for (i = 0; i < screen_res->noutput; i++) {
		output_info = XRRGetOutputInfo(xdisplay, screen_res, screen_res->outputs[i]);
		if (output_info->connection == RR_Connected) {
			g_print("   [%s] is connected\n", output_info->name);
			player->displaytype = tegra_display_name_to_type(output_info->name);
		}
		XRRFreeOutputInfo(output_info);
	}

	XRRFreeScreenResources(screen_res);
	return 0;
}

static int player_change_state(struct media_player *player, GstState state)
{
	GstStateChangeReturn ret;

	if (!player || !player->pipeline)
		return -EINVAL;

	ret = gst_element_set_state(player->pipeline, state);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		g_critical("failed to set player state");
		return -ECANCELED;
	}

	return 0;
}

/**
 * HERE comes the part for remote-control
 */
int media_player_create(struct media_player **playerp)
{
	struct media_player *player = NULL;
	int ret = -1;

	if (!playerp)
		return -EINVAL;

	g_debug("   allocation memory...");
	player = g_new0(struct media_player, 1);
	if (player == NULL) {
		g_warning("out of memory\n");
		return -ENOMEM;
	}

	g_debug("   getting display type...");
	ret = player_find_display_type(player);
	if (ret < 0) {
		g_warning("unable to query display type %d", ret);
		player->displaytype = NV_DISPLAY_TYPE_DEFAULT;
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
	ret = player_window_init(player);
	if (ret < 0) {
		g_warning("failed to create window %d", ret);
		goto err;
	}

	g_debug("   assigning player...");
	*playerp = player;
	return 1;

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
	enum media_player_state state = MEDIA_PLAYER_STOPPED;
	int err = 0;

	g_debug("> %s(uri=%s)", __func__, uri);

	if (!player || !uri)
		return -EINVAL;

	if (!g_str_has_prefix(uri, "udp://") &&
	    !g_str_has_prefix(uri, "file://") &&
	    !g_str_has_prefix(uri, "http://"))
	{
		g_warning("  unsupported uri");
		err = -EINVAL;
		goto out;
	}

	err = media_player_get_state(player, &state);
	if (err < 0)
		g_warning("  unable to get state");
	/* because url can be valid, but we do not know the content, we can
	 * not be sure that the chain can handle the new url we destroy the
	 * old and create a new. this is the safest way */
	g_debug("   creating new pipeline...");
	if (player_create_pipeline(player, (const gchar*)uri) < 0) {
		g_critical("  failed to create pipeline");
		err = -ENOSYS;
	} else {
		err = player_change_state(player, state == MEDIA_PLAYER_STOPPED
			? GST_STATE_PAUSED : GST_STATE_PLAYING);
	}
out:
	g_debug("< %s()", __func__);
	return err;
}

int media_player_get_uri(struct media_player *player, char **urip)
{
	if (!urip || !player || !player->pipeline)
		return -EINVAL;

	*urip = strdup(player->uri);
	return urip == NULL ? -ENOMEM : 0;
}

int media_player_set_output_window(struct media_player *player,
                                   unsigned int x, unsigned int y,
                                   unsigned int width,
                                   unsigned int height)
{
	GdkScreen *screen;
	int scale;

	if (!player)
		return -EINVAL;

	g_debug("> %s(player=%p, x=%d, y=%d, width=%d, height=%d)",
		__func__, player, x, y, width, height);

	/* assign the new parameters to our window, in the future this should
	 * should be sufficient, but since we can not assign our window to the
	 * gstreamer plugin we need to do this seperatly */
	if (player->window) {
		gdk_window_move_resize(player->window, x, y, width, height);
//		gdk_window_clear(player->window);
	}

	scale = player->scale;

	screen = gdk_screen_get_default();
	if ((width >= gdk_screen_get_width(screen)) &&
	    (height >= gdk_screen_get_height(screen)))
		player->scale = SCALE_FULLSCREEN;
	else
		player->scale = SCALE_PREVIEW;

	/* if the scale has changed we need to destroy the pipeline in order
	 * to change the videosink we use.
	 * we do this to get better scaling quality for fullscreen */
	if (scale != player->scale && player->pipeline) {
		GstState state = MEDIA_PLAYER_STOPPED;
		gchar *uri;
		int ret;

		player_dump(player);
		g_debug("   scale has changed! %s", player->uri);

		ret = gst_element_get_state(player->pipeline, &state, NULL, GST_SECOND);
		if (ret != GST_STATE_CHANGE_SUCCESS) {
			g_warning("unable to get state");
			state = MEDIA_PLAYER_PAUSED;
		}

		uri = g_strdup(player->uri);
		ret = player_create_pipeline(player, uri);
		ret = player_change_state(player, state);
		g_free(uri);
	}

	/* inform the gstreamer output window about the change. this step will
	 * be removed as soon as the  have-x-window-handle or
	 * prepare-x-window-handle notification is working */
	if (player->scale != SCALE_FULLSCREEN && player->pipeline)
		player_window_move(player, x, y, width, height);

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
#endif

	/* update window pos */
	player_window_update(player);
	return ret;
}

int media_player_stop(struct media_player *player)
{
	int ret;
	if (!player)
		return -EINVAL;

#if HAVE_SOFTWARE_DECODER
	ret = player_change_state(player, GST_STATE_PAUSED);

	gdk_threads_enter();
	gdk_window_hide(player->window);
	gdk_threads_leave();
#else
	gchar *uri = g_strdup(player->uri);
	g_debug(" uri=%s", uri);
	ret = player_destroy_pipeline(player);
	player->uri = uri;
#endif
	return ret;
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
