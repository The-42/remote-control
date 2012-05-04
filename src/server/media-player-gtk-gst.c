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

#if defined(ENABLE_XRANDR)
#include <X11/extensions/Xrandr.h>
#endif

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
	bool have_nv_omx;

	bool enable_fixed_fullscreen; /* force a fixed fullscreen resolution */
	int fullscreen_width;
	int fullscreen_height;

	gchar **preferred_languages;
};

#if 0
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
	g_printf("   have_nv_omx:......... %d\n", player->have_nv_omx);
}
#endif

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

static void player_source_setup(GstElement *playbin, GstElement *source,
								gpointer user_data)
{
	GObjectClass *source_class = NULL;
	gchar *uri = NULL;
	user_data = NULL;

	source_class = G_OBJECT_GET_CLASS (source);
	if (!g_object_class_find_property (source_class, "is-live"))
		return;

	g_object_get(playbin, "uri", &uri, NULL);
	g_print("source-setup: %s, uri: %s\n", GST_ELEMENT_NAME(source), uri);
	if(!g_ascii_strncasecmp(uri, "http://", 7)) {
		g_print("Playing http source, assume it's a live source\n");
		g_object_set(G_OBJECT(source), "is-live", true, "do-timestamp", true,
					 NULL);
	}
	g_free(uri);
}

static int player_get_language_code_priority(struct media_player *player,
                                             const gchar *language_code)
{
	gchar **language = player->preferred_languages;
	int priority = 0;

	if (language == NULL)
		return -1;

	while (*language) {
		if (g_ascii_strcasecmp(language_code, *language) == 0)
			return priority;

		priority++;
		language++;
	}

	return -1;
}

static void player_check_audio_tracks(GstElement *playbin,
                                      gpointer user_data)
{
	struct media_player *player = (struct media_player*)user_data;
	int track_priority = INT_MAX;
	gchar *language_code = NULL;
	GstTagList *taglist = NULL;
	int select_track = -1;
	guint n_audio;
	int priority;
	int i;

	g_object_get(G_OBJECT(playbin), "n-audio", &n_audio, NULL);
	for (i = 0; i < n_audio; i++) {
		g_signal_emit_by_name(playbin, "get-audio-tags", i, &taglist);
		if (!taglist)
			continue;

		gst_structure_get (GST_STRUCTURE(taglist),
		                   "language-code", G_TYPE_STRING,
		                   &language_code, NULL);

		priority = player_get_language_code_priority(player,
		                                             language_code);
		if (-1 < priority && priority < track_priority) {
			track_priority = priority;
			select_track = i;
		}

		gst_tag_list_free(taglist);
		g_free(language_code);
	}

	if (select_track != -1) {
		g_debug("Select audio track %d", select_track);
		g_object_set(G_OBJECT(playbin), "current-audio",
		             select_track, NULL);
	}
}

static void set_webkit_appsrc_rank(guint rank)
{
	GstRegistry *registry = gst_registry_get_default();
	GstPluginFeature *feature;

	/* FIXME: the appsrc is used by webkit to provide there own httpsrc,
	 *        and this source is buggy and has a memory leak. */
	feature = gst_registry_lookup_feature(registry, "appsrc");
	if (feature) {
		g_debug("   update %s priority", gst_plugin_feature_get_name(feature));
		gst_plugin_feature_set_rank(feature, rank);
		gst_object_unref(feature);
	}

	feature = gst_registry_lookup_feature(registry, "webkitwebsrc");
	if (feature) {
		g_debug("   update %s priority", gst_plugin_feature_get_name(feature));
		gst_plugin_feature_set_rank(feature, rank);
		gst_object_unref(feature);
	}
}

static void handle_message_state_change(struct media_player *player, GstMessage *message)
{
	GstState pending = GST_STATE_VOID_PENDING;
	GstState old_state = GST_STATE_VOID_PENDING;
	GstState new_state = GST_STATE_VOID_PENDING;

	gst_message_parse_state_changed(message, &old_state, &new_state, &pending);

	if (GST_MESSAGE_SRC(message) != GST_OBJECT(player->pipeline))
		return;

	g_printf("   Element %s changed state from %s to %s (pending=%s)\n",
		GST_OBJECT_NAME(message->src),
		gst_element_state_get_name(old_state),
		gst_element_state_get_name(new_state),
		gst_element_state_get_name(pending));

	switch (new_state) {
	case GST_STATE_PLAYING:
		set_webkit_appsrc_rank(GST_RANK_PRIMARY + 100);
		player_check_audio_tracks(player->pipeline, player);
		break;
	default:
		break;
	}
}

static int handle_message_error(struct media_player *player, GstMessage *message)
{
	GError *err = NULL;
	gchar *debug = NULL;

	gst_message_parse_error(message, &err, &debug);
	g_printf("ERROR from element %s: %s\n",
			 GST_OBJECT_NAME (message->src), err->message);
	if (err->domain == GST_STREAM_ERROR) {
		/* try to restart the pipeline */
		gst_element_set_state(player->pipeline, GST_STATE_PLAYING);
	}
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

static gboolean is_live_source (GstElement *source)
{
  GObjectClass *source_class = NULL;
  gboolean is_live = FALSE;

  source_class = G_OBJECT_GET_CLASS (source);
  if (!g_object_class_find_property (source_class, "is-live"))
	return FALSE;

  g_object_get (G_OBJECT (source), "is-live", &is_live, NULL);

  return is_live;
}

static void handle_message_buffering(struct media_player *player, GstMessage *message)
{
	GstState state = GST_STATE_NULL;
	GstElement *source = NULL;
	gboolean is_live = true;
	gint percent = 0;

	gst_message_parse_buffering (message, &percent);

	g_object_get(player->pipeline, "source", &source, NULL);
	is_live = is_live_source(source);

	if (!is_live && percent < 100 && state == GST_STATE_PLAYING) {
		g_print("Go to PAUSED for buffering\n");
		gst_element_set_state(player->pipeline, GST_STATE_PAUSED);
	} else if(!is_live && percent >= 100 && state == GST_STATE_PAUSED) {
		g_print("Go to PLAYING as buffering completed\n");
		gst_element_set_state(player->pipeline, GST_STATE_PLAYING);
	}
	g_object_unref(source);
}

static gboolean player_set_x_overlay(struct media_player *player)
{
	GstElement *video_sink;

	g_debug(" > %s()", __func__);
	video_sink = gst_bin_get_by_name(GST_BIN(player->pipeline), "video-out");
	if (!video_sink) {
		g_warning("< %s(): not found", __func__);
		return FALSE;
	}

	if (GDK_IS_WINDOW(player->window)) {
		g_debug("   gst_x_overlay_set_window_handle()....");
		gst_x_overlay_set_window_handle(GST_X_OVERLAY(video_sink),
				GDK_WINDOW_XWINDOW(player->window));
		gdk_window_freeze_updates (player->window);
	} else {
		g_debug("   window not ready");
		gst_x_overlay_set_window_handle(GST_X_OVERLAY(video_sink),
				player->xid);
	}

	/* force redraw */
	gst_x_overlay_expose(GST_X_OVERLAY(video_sink));

	gst_object_unref(video_sink);
	return TRUE;
}

static gboolean player_set_x_window_id(struct media_player *player,
                                       const GValue *value)
{
	if (!value)
		return FALSE;

#ifndef __x86_64__
	if (G_VALUE_HOLDS_POINTER(value)) {
		player->xid = (XID)((guintptr)g_value_get_pointer(value));
	}
#endif
	if (G_VALUE_HOLDS_ULONG(value)) {
		player->xid = (XID)g_value_get_ulong(value);
	}

	g_printf(" < %s()", __func__);
	return player->xid != 0;
}

static void player_show_output(struct media_player *player, gboolean show)
{
	if (!player->have_nv_omx && !player->xid && player->window != NULL) {
		gdk_threads_enter();
		if (show) {
			gdk_window_show(player->window);
		} else if (gdk_window_is_visible(player->window)) {
			gdk_window_hide(player->window);
			gdk_window_thaw_updates (player->window);
			gdk_window_clear(player->window);
		}
		gdk_threads_leave();
	} else {
		if (!show && gdk_window_is_visible(player->window)) {
			gdk_threads_enter();
			gdk_window_hide(player->window);
			gdk_threads_leave();
		}
	}
}

static int tegra_omx_window_move(struct media_player *player,
                              int x, int y, int width, int height)
{
	GstElement *video;
	video = gst_bin_get_by_name(GST_BIN(player->pipeline), "video-output");
	if (!video) {
		g_warning("   no element video-output found");
		return -ENODATA;
	}

	/* the height must be the last parameter we set, this is
	 * because the plugin only updates x,y,w when h is set */
	g_object_set(G_OBJECT(video), "output-pos-x", x,
	             "output-pos-y", y, "output-size-width", width,
	             "output-size-height", height, NULL);
	gst_object_unref(video);
	return 0;
}

static int player_window_update(struct media_player *player)
{
	gint x, y, width, height;

	if (!player || !player->window)
		return -EINVAL;

	gdk_window_get_position(GDK_WINDOW(player->window), &x, &y);
	gdk_drawable_get_size(GDK_DRAWABLE(player->window), &width, &height);

	if (player->have_nv_omx)
		return tegra_omx_window_move(player, x, y, width, height);

	g_debug("      going to resize.");
	if (player->xid) {
		GdkDisplay *display = gdk_display_get_default();
		Display *xdisplay =  gdk_x11_display_get_xdisplay(display);
		int err;
		err = XMoveResizeWindow(xdisplay, player->xid, x, y, width, height);
		if (err < 0)
			g_warning("failed to move window: %d", err);
	}
	return 0;
}

static gboolean player_element_message_sync(GstBus *bus, GstMessage *msg,
                                            struct media_player *player)
{
	gboolean ret = FALSE;
	const gchar* name;

	if (!msg->structure)
		return FALSE;

	name = gst_structure_get_name(msg->structure);
	if (g_strcmp0(name, "prepare-xwindow-id") == 0) {
		g_debug("    prepare-xwindow-id");
		ret = player_set_x_overlay(player);
	}
	else if (g_strcmp0(name, "have-xwindow-id") == 0) {
		g_debug("    have-xwindow-id");
		ret = player_set_x_window_id(player,
			gst_structure_get_value(msg->structure, "xwindow-id"));
	}
	return ret;
}

static gboolean player_gst_bus_event(GstBus *bus, GstMessage *msg, gpointer data)
{
	struct media_player *player = data;

	switch (GST_MESSAGE_TYPE(msg)) {
	case GST_MESSAGE_UNKNOWN:
		g_debug("End of stream");
		player->state = MEDIA_PLAYER_STOPPED;
		break;
	case GST_MESSAGE_EOS:
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
	case GST_MESSAGE_TAG:
		break;
	case GST_MESSAGE_BUFFERING:
		handle_message_buffering(player, msg);
		break;
	case GST_MESSAGE_STATE_CHANGED:
		handle_message_state_change(player, msg);
		break;
	case GST_MESSAGE_STATE_DIRTY:
		g_warning("Stream is dirty\n");
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
	case GST_MESSAGE_APPLICATION:
		break;
	case GST_MESSAGE_ELEMENT:
		player_element_message_sync(bus, msg, player);
		break;
	case GST_MESSAGE_SEGMENT_START:
		break;
	case GST_MESSAGE_SEGMENT_DONE:
		break;
	case GST_MESSAGE_DURATION:
		break;
	case GST_MESSAGE_LATENCY:
		break;
	case GST_MESSAGE_ASYNC_START:
		break;
	case GST_MESSAGE_ASYNC_DONE:
		break;
	case GST_MESSAGE_REQUEST_STATE:
		break;
	case GST_MESSAGE_STEP_START:
		break;
	case GST_MESSAGE_QOS:
		break;
	case GST_MESSAGE_PROGRESS:
		break;
	default:
		break;
	}
	return TRUE;
}

static GstBusSyncReply player_gst_bus_sync_handler(GstBus *bus,
                                                   GstMessage *message,
                                                   gpointer user_data)
{
	struct media_player *player = user_data;
	GstMessageType type = GST_MESSAGE_TYPE(message);
	GstBusSyncReply ret = GST_BUS_PASS;

	switch (type) {
	case GST_MESSAGE_ELEMENT:
		if (player_element_message_sync(bus, message, player)) {
			g_debug("** showing window");
			player_show_output(player, TRUE);
			g_debug("** updating window");
			player_window_update(player);
			gst_message_unref(message);
			ret = GST_BUS_DROP;
		}
		break;

	case GST_MESSAGE_EOS:
		player_show_output(player, FALSE);
		g_debug("**  hiding window %s",
			ret == GST_BUS_PASS ? "" : "due error");
		break;

	case GST_MESSAGE_STREAM_STATUS:
	case GST_MESSAGE_STATE_CHANGED:
	case GST_MESSAGE_TAG:
	case GST_MESSAGE_QOS:
	case GST_MESSAGE_BUFFERING:
	case GST_MESSAGE_WARNING:
		/* silently ignore */
		break;

	default:
		g_printf("   ignore: %s\n",
		         gst_message_type_get_name(type));
		break;
	}

	return ret;
}

static int player_window_init(struct media_player *player)
{
	GdkWindowAttr attributes = {
		.x = 0, .y = 0, .width = 854, .height = 480,
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
	if (player->xid)
		player->xid = 0;

	return 0;
}

static int player_create_software_pipeline(struct media_player *player, const gchar* uri)
{
#define PIPELINE \
	"playbin2 " \
		"video-sink=\"glessink name=video-out\" " \
		"audio-sink=\"alsasink name=audio-out device=%s\" " \
		"flags=0x00000160 " \
		"uri=%s"

	GError *error = NULL;
	GstBus *bus;
	gchar *pipe;
	const gchar *ad;
	int ret = -EINVAL;

	if (player->pipeline)
		player_destroy_pipeline(player);

	/* for HDMI we need to select the correct audio device */
	ad = player->displaytype == NV_DISPLAY_TYPE_HDMI ? "hdmi" : "default";
	pipe = g_strdup_printf(PIPELINE, ad, uri);

	set_webkit_appsrc_rank(GST_RANK_NONE);

	player->pipeline = gst_parse_launch_full(pipe, NULL,
	                        GST_PARSE_FLAG_FATAL_ERRORS, &error);
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

	g_signal_connect (player->pipeline, "audio-changed",
	                  G_CALLBACK (player_check_audio_tracks),
	                  player);

	g_signal_connect (player->pipeline, "source-setup",
					  G_CALLBACK (player_source_setup),
					  NULL);

cleanup:
	if (error)
		g_error_free(error);
	g_free(pipe);
	return ret;
}

static int player_create_nvidia_pipeline(struct media_player *player, const gchar* uri)
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

	if (player->have_nv_omx) {
		g_debug("   building nvidia pipe...");
		ret = player_create_nvidia_pipeline(player, uri);
	} else {
		g_debug("   building software pipe...");
		ret = player_create_software_pipeline(player, uri);
	}

	if (ret < 0 && player->pipeline) {
		if (player->uri) {
			g_free(player->uri);
			player->uri = NULL;
		}
	}

	if (!player->uri)
		player->uri = g_strdup(uri);

	g_debug(" < %s(): %d", __func__, ret);
	return ret;
}

static int player_init_gstreamer(struct media_player *player)
{
	GstRegistry *registry = gst_registry_get_default();
	GstPluginFeature *feature;
	GError *err = NULL;
	/* FIXME: we need the real arguments, otherwise we can not pass things
	 *        like GST_DEBUG=2 ??? */
	char *argv[] = { "remote-control", NULL };
	int argc = 1;

	/* we need to fake the args, because we have no access from here */
	g_debug("   initialize gstreamer...");
	if (!gst_init_check(&argc, (char***)&argv, &err)) {
		g_error_free(err);
		g_critical("failed to initialize gstreamer");
		return -ENOSYS;
	}

	feature = gst_registry_lookup_feature(registry, "nv_gl_videosink");
	if (feature) {
		gst_object_unref(feature);
		player->have_nv_omx = 1;
	}

	g_debug("   omx plugin %sfound", player->have_nv_omx ? "" : "not ");

	return 0;
}

#if defined(ENABLE_XRANDR)
	/* only valid for vibrante */
static int tegra_display_name_to_type(const gchar *name)
{
	struct connection_map {
		const gchar *name;
		int type;
	};

	static const struct connection_map MAP[] = {
		{ "CRT",    NV_DISPLAY_TYPE_CRT  },
		{ "HDMI",   NV_DISPLAY_TYPE_HDMI },
		{ "HDMI-1", NV_DISPLAY_TYPE_HDMI },
		{ "TFTLCD", NV_DISPLAY_TYPE_LVDS },
		{ "LVDS-1", NV_DISPLAY_TYPE_LVDS }
	};
	int index;

	for (index = 0; index < G_N_ELEMENTS(MAP); index++) {
		if (g_strcmp0(name, MAP[index].name) == 0)
			return MAP[index].type;
	}

	return NV_DISPLAY_TYPE_DEFAULT;
}
#endif

static int player_find_display_type(struct media_player *player)
{
#if defined(ENABLE_XRANDR)
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
#endif
	return 0;
}

static int player_change_state(struct media_player *player, GstState state,
                               gboolean sync)
{
	GstStateChangeReturn ret;
	int i;

	if (!player || !player->pipeline)
		return -EINVAL;

	ret = gst_element_set_state(player->pipeline, state);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		g_critical("failed to set player state");
		return -ECANCELED;
	}

	/* When set state reports ASYNC, we wait till we get a useful result.
	 * But we do not wait forever, since there are situations where the
	 * we always get ASYNC as result */
	if (sync && ret == GST_STATE_CHANGE_ASYNC) {
		for (i=0; i<3; i++) {
			ret = gst_element_get_state(player->pipeline, NULL,
			                            NULL, GST_SECOND);
			if (ret == GST_STATE_CHANGE_FAILURE) {
				g_critical("%s: failed to get player state", __func__);
				return -ECANCELED;
			}

			if (ret != GST_STATE_CHANGE_ASYNC)
				break;
			sleep(1); /* re-schedule */
		}
	}
	return 0;
}

#if defined(ENABLE_XRANDR)
static int player_xrandr_configure_screen(struct media_player *player,
                                          int width, int height, int rate)
{
	XRRScreenConfiguration *conf;
	XRRScreenResources *screen_res;
	XRRScreenSize *available_sizes;
	Rotation original_rotation;
	short *available_rates;
	SizeID current_size_id;
	short current_rate;
	int event_base = 0;
	int error_base = 0;
	Display *display;
	XID rootwindow;
	int gst_ret;
	Status ret;
	int nsizes;
	int nrates;
	int i;

	display = GDK_WINDOW_XDISPLAY (player->window);

	if (!XRRQueryExtension(display, &event_base, &error_base)) {
		g_warning("randr not available: %d %d", event_base, error_base);
		return -ENOSYS;
	}

	rootwindow = gdk_x11_drawable_get_xid(GDK_DRAWABLE (player->window));
retry_config:
	XLockDisplay(display);
	conf = XRRGetScreenInfo (display, rootwindow);
	screen_res = XRRGetScreenResources (display, rootwindow);
	if (!screen_res) {
		XUnlockDisplay(display);
		g_warning("no screen resource");
		return -ENOMEM;
	}

	/* try to find a screen size matching the desired size */
	available_sizes = XRRConfigSizes(conf, &nsizes);
	current_size_id = XRRConfigCurrentConfiguration (conf, &original_rotation);

	if (width == -1 && height == -1) {
		current_size_id = 0;
	} else if (available_sizes[current_size_id].width != width ||
	           available_sizes[current_size_id].height != height)
	{
		for (i = 0; i < nsizes; i++, available_sizes++) {
			if (available_sizes->width == width &&
			    available_sizes->height == height) {
				current_size_id = i;
				break;
			}
		}
	}

	/* try to find a matching rate */
	available_rates = XRRRates (display, 0, current_size_id, &nrates);
	current_rate = *available_rates;
	for (i = 0; i < nrates; i++, available_rates++) {
		if (*available_rates == rate) {
			current_rate = rate;
			break;
		}
	}

	/* pause gstreamer, to avoid flow errors */
	gst_ret = player_change_state(player, GST_STATE_READY, true);
	if (gst_ret < 0)
		g_warning ("Failed to pause playback before mode-switch");

	g_debug ("   xrandr: switch to %dx%d/%dHz\n", available_sizes->width,
			 available_sizes->height, current_rate);
	ret = XRRSetScreenConfigAndRate (display, conf, rootwindow,
	                                 current_size_id, original_rotation,
	                                 current_rate, CurrentTime);
	if (ret != RRSetConfigSuccess) {
		g_warning ("xrandr: could not change display settings: %d", ret);
		if(ret == RRSetConfigInvalidConfigTime) {
			XRRFreeScreenConfigInfo(conf);
			XRRFreeScreenResources(screen_res);
			XUnlockDisplay(display);
			goto retry_config;
		}
	}

	XRRFreeScreenConfigInfo (conf);
	XRRFreeScreenResources (screen_res);
	XUnlockDisplay(display);

	/* resume gstreamer playback */
	gst_ret = player_change_state(player, GST_STATE_PLAYING, false);
	if (gst_ret < 0)
		g_warning ("Failed to resume playback after mode-switch");

	return 0;
}
#endif

static void media_player_load_config(struct media_player *player, GKeyFile *config)
{
	GError *err = NULL;

	if (!g_key_file_has_group(config, "media-player"))
		g_debug("media-player-gtk-gst: no configuration for media-player found");

	player->preferred_languages = g_key_file_get_string_list(config,
			"media-player", "preferred-languages", NULL, NULL);
	g_debug("Preferred languages: %p\n", player->preferred_languages);

	player->enable_fixed_fullscreen = true;

	player->fullscreen_width = g_key_file_get_integer(config,
			"media-player", "fullscreen-width", &err);
	if (err != NULL) {
		player->enable_fixed_fullscreen = false;
		g_error_free(err);
		err = NULL;
	}

	player->fullscreen_height = g_key_file_get_integer(config,
			"media-player", "fullscreen-height", &err);
	if (err != NULL) {
		player->enable_fixed_fullscreen = false;
		g_error_free(err);
		err = NULL;
	}
}

/**
 * HERE comes the part for remote-control
 */
int media_player_create(struct media_player **playerp, GKeyFile *config)
{
	struct media_player *player = NULL;
	int ret = -1;

	if (!playerp)
		return -EINVAL;

	player = g_new0(struct media_player, 1);
	if (player == NULL)
		return -ENOMEM;

	media_player_load_config(player, config);

	ret = player_init_gstreamer(player);
	if (ret < 0) {
		g_warning("%s: initialize gstreamer failed %d", __func__, ret);
		goto err;
	}

	ret = player_find_display_type(player);
	if (ret < 0) {
		g_warning("%s: unable to query display type %d", __func__, ret);
		player->displaytype = NV_DISPLAY_TYPE_DEFAULT;
	}

	ret = player_window_init(player);
	if (ret < 0) {
		g_warning("%s: failed to create window %d", __func__, ret);
		goto err;
	}

	ret = player_create_pipeline(player, "none");
	if (ret < 0 || player->pipeline == NULL) {
		g_warning("%s: failed to setup pipeline", __func__);
		goto err;
	}

	*playerp = player;
	return 1;

err:
	g_critical("%s: creating player failed %d", __func__, ret);
	return ret;
}

int media_player_free(struct media_player *player)
{
	if (!player)
		return -EINVAL;

	g_strfreev(player->preferred_languages);
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

	err = media_player_get_state(player, &state);
	if (err < 0)
		g_warning("   unable to get state");

	set_webkit_appsrc_rank(GST_RANK_NONE);

	/* because url can be valid, but we do not know the content, we can
	 * not be sure that the chain can handle the new url we destroy the
	 * old and create a new. this is the safest way */
	g_debug("   destroy old pipeline...");
	if (player->pipeline) {
		g_warning("reuse old pipeline");
		gst_element_set_state(player->pipeline, GST_STATE_READY);
		g_object_set(player->pipeline, "uri", (const gchar*)uri, NULL);
		err = player_change_state(player, state == MEDIA_PLAYER_STOPPED
								  ? GST_STATE_PAUSED : GST_STATE_PLAYING, false);
	} else {
		if (player_create_pipeline(player, (const gchar*)uri) < 0) {
			g_critical("  failed to create pipeline");
			err = -ENOSYS;
		} else {
			err = player_change_state(player, state == MEDIA_PLAYER_STOPPED
									  ? GST_STATE_PAUSED : GST_STATE_PLAYING, false);
		}
	}

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

	g_debug("> %s(player=%p, x=%d, y=%d, width=%d, height=%d)",
		__func__, player, x, y, width, height);

	if (!player)
		return -EINVAL;

	screen = gdk_screen_get_default();

#if defined(ENABLE_XRANDR)
	if (player->enable_fixed_fullscreen &&
	    (width >= gdk_screen_get_width(screen)) &&
	    (height >= gdk_screen_get_height(screen)))
	{
		if (player->scale != SCALE_FULLSCREEN) {
			width = player->fullscreen_width;
			height = player->fullscreen_height;

			player_xrandr_configure_screen (player, width, height, 50);
			player->scale = SCALE_FULLSCREEN;
		}
	} else if (player->scale != SCALE_PREVIEW) {
		/* switch back to default rasolution */
		player_xrandr_configure_screen (player, -1, -1, 50);
		player->scale = SCALE_PREVIEW;
	}
#endif
	if (player->xid) {
		GdkDisplay *display = gdk_display_get_default();
		Display *xdisplay =  gdk_x11_display_get_xdisplay(display);
		g_debug("   move to %dx%d and resize to: %dx%d", x, y, width, height);
		XMoveResizeWindow(xdisplay, player->xid, x, y, width, height);
	}
	/* assign the new parameters to our window, in the future this
	 * should be sufficient, but since we can not assign our window
	 * to the gstreamer plugin we need to do this seperatly */
	if (player->window) {
		gdk_threads_enter();
		gdk_window_move_resize(player->window, x, y, width, height);
		gdk_window_clear(player->window);
		gdk_threads_leave();
	}

	if (player->have_nv_omx) {
		int scale = player->scale;

		if ((width >= gdk_screen_get_width(screen)) &&
		    (height >= gdk_screen_get_height(screen)))
			player->scale = SCALE_FULLSCREEN;
		else
			player->scale = SCALE_PREVIEW;

		/* if the scale has changed we need to destroy the pipeline in
		 * order to change the videosink we use.
		 * we do this to get better scaling quality for fullscreen */
		if (scale != player->scale && player->pipeline) {
			GstState state = MEDIA_PLAYER_STOPPED;
			gchar *uri;
			int ret;

			g_debug("   scale has changed! %s", player->uri);
			ret = gst_element_get_state(player->pipeline,
			                            &state, NULL, GST_SECOND);
			if (ret != GST_STATE_CHANGE_SUCCESS) {
				g_warning("unable to get state");
				state = MEDIA_PLAYER_PAUSED;
			}

			uri = g_strdup(player->uri);
			ret = player_create_pipeline(player, uri);
			ret = player_change_state(player, state, false);
			g_free(uri);
		}

		/* inform the gstreamer output window about the change. this
		 * step will be removed as soon as the  have-x-window-handle
		 * or prepare-x-window-handle notification is working */
		if (player->scale != SCALE_FULLSCREEN && player->pipeline)
			tegra_omx_window_move(player, x, y, width, height);
	}

	g_debug("< %s()", __func__);
	return 0;
}

int media_player_play(struct media_player *player)
{
	int ret = player_change_state(player, GST_STATE_PLAYING, false);
	if (ret < 0) {
		gchar *uri = g_strdup(player->uri);
		ret = media_player_set_uri(player, player->uri);
		g_free(uri);
		if (ret < 0)
			return ret;
	}
	return ret;
}

int media_player_stop(struct media_player *player)
{
	gchar *uri;
	int ret;

	if (!player)
		return -EINVAL;

	g_debug("   saving old uri");
	uri = g_strdup(player->uri);
	g_debug("   uri=[%s]", uri);
	g_debug("   hiding window");
	player_show_output(player, FALSE);
	g_debug("   destroying pipeline");
	ret = player_destroy_pipeline(player);
	player->uri = uri;

	return ret;
}

int media_player_pause(struct media_player *player)
{
	g_return_val_if_fail(player != NULL, -EINVAL);

	return player_change_state (player, GST_STATE_PAUSED, false);
}

int media_player_resume(struct media_player *player)
{
	g_return_val_if_fail(player != NULL, -EINVAL);

	return player_change_state (player, GST_STATE_PLAYING, false);
}

int media_player_get_duration(struct media_player *player,
		unsigned long *duration)
{
	GstQuery *query;
	gboolean res;

	g_return_val_if_fail(player != NULL, -EINVAL);

	query = gst_query_new_duration (GST_FORMAT_TIME);
	res = gst_element_query (player->pipeline, query);
	if (res) {
		gint64 duration_ns;
		gst_query_parse_duration (query, NULL, &duration_ns);
		*duration = duration_ns / GST_MSECOND;
	} else
		return -EINVAL;
	gst_query_unref (query);

	return 0;
}

int media_player_get_position(struct media_player *player,
		unsigned long *position)
{
	GstQuery *query;
	gboolean res;

	g_return_val_if_fail(player != NULL, -EINVAL);

	query = gst_query_new_position (GST_FORMAT_TIME);
	res = gst_element_query (player->pipeline, query);
	if (res) {
		gint64 position_ns;
		gst_query_parse_position (query, NULL, &position_ns);
		*position = position_ns / GST_MSECOND;
	} else
		return -EINVAL;
	gst_query_unref (query);

	return 0;
}

int media_player_set_position(struct media_player *player,
		unsigned long position)
{
	g_return_val_if_fail(player != NULL, -EINVAL);

	if (!gst_element_seek(player->pipeline, 1.0, GST_FORMAT_TIME,
	                      GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET,
	                      position * GST_MSECOND, GST_SEEK_TYPE_NONE,
	                      GST_CLOCK_TIME_NONE))
		return -EINVAL;

	return 0;
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
