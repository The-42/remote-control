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

#include <gst/gst.h>

#include "remote-control-stub.h"
#include "remote-control.h"

#if GST_CHECK_VERSION(1, 0, 0)
#  define PLAYBIN_ELEMENT "playbin"
#else
#  define PLAYBIN_ELEMENT "playbin2"
#endif

struct sound_manager {
	GstElement *play;
	GstBus *bus;
};

static int handle_message_error(struct sound_manager *manager, GstMessage *message)
{
	GError *err;
	gchar *debug;

	gst_message_parse_error(message, &err, &debug);
	g_warning("sound-manager error: %s", err->message);

	g_error_free(err);
	g_free(debug);

	return 0;
}

static gboolean sound_manger_gst_bus_event(GstBus *bus, GstMessage *msg,
										   gpointer data)
{
	struct sound_manager *manager = data;

	switch (GST_MESSAGE_TYPE(msg)) {
	case GST_MESSAGE_UNKNOWN:
	case GST_MESSAGE_EOS:
		g_debug("End of stream");
		gst_element_set_state (manager->play, GST_STATE_NULL);
		break;
	case GST_MESSAGE_ERROR:
		handle_message_error(manager, msg);
		break;
	default:
		break;
	}
	return TRUE;
}

int sound_manager_create(struct sound_manager **managerp, struct audio *audio,
		GKeyFile *config)
{
	struct sound_manager *manager;
	char *argv[] = { "remote-control", NULL };
	int argc = 1;
	GError *err;

	if (!managerp)
		return -EINVAL;

	manager = g_malloc0(sizeof(*manager));
	if (!manager)
		return -ENOMEM;

	/* we need to fake the args, because we have no access from here */
	g_debug(" %s():  initialize gstreamer...", __func__);
	if (!gst_init_check(&argc, (char***)&argv, &err)) {
		g_error_free(err);
		g_critical("failed to initialize gstreamer");
		return -ENOSYS;
	}

	/* create the playbin instance */
	manager->play = gst_element_factory_make(PLAYBIN_ELEMENT, "play");
	if (!manager->play) {
		g_free(manager);
		return -ENOSYS;
	}

	manager->bus = gst_pipeline_get_bus(GST_PIPELINE(manager->play));
	gst_bus_add_watch(manager->bus, (GstBusFunc)sound_manger_gst_bus_event,
					  manager);

	*managerp = manager;
	return 0;
}

int sound_manager_free(struct sound_manager *manager)
{
	if (!manager)
		return -EINVAL;

	gst_element_set_state (manager->play, GST_STATE_NULL);
	gst_object_unref (GST_OBJECT (manager->bus));
	gst_object_unref (GST_OBJECT (manager->play));
	g_free(manager);
	return 0;
}

int sound_manager_play(struct sound_manager *manager, const char *uri)
{
	enum GstStateChangeReturn ret;

	if (!manager || !uri) {
		g_warning("%s(%p, %p): manager or uri invalid", __func__, manager,
				  uri);
		return -EINVAL;
	}

	ret = gst_element_set_state(manager->play, GST_STATE_NULL);
	if (ret == GST_STATE_CHANGE_FAILURE)
		return -EINVAL;

	g_object_set (G_OBJECT (manager->play), "uri", uri, NULL);

	ret = gst_element_set_state(manager->play, GST_STATE_PLAYING);

	return ret != GST_STATE_CHANGE_FAILURE ? 0 : -EINVAL;
}

int sound_manager_pause(struct sound_manager *manager)
{
	enum sound_manager_state state;
	enum GstStateChangeReturn ret;
	int err;

	err = sound_manager_get_state(manager, &state);
	if (err)
		return err;

	if (state == SOUND_MANAGER_PLAYING)
		ret = gst_element_set_state(manager->play, GST_STATE_PAUSED);
	else if (state == SOUND_MANAGER_PAUSED)
		ret = gst_element_set_state(manager->play, GST_STATE_PLAYING);
	else
		ret = GST_STATE_CHANGE_SUCCESS;

	return ret != GST_STATE_CHANGE_FAILURE ? 0 : -EINVAL;
}

int sound_manager_stop(struct sound_manager *manager)
{
	enum GstStateChangeReturn ret;

	if (!manager) {
		g_warning("%s(%p): manager uri invalid", __func__, manager);
		return -EINVAL;
	}

	ret = gst_element_set_state(manager->play, GST_STATE_NULL);

	return ret != GST_STATE_CHANGE_FAILURE ? 0 : -EINVAL;
}

int sound_manager_get_state(struct sound_manager *manager,
		enum sound_manager_state *statep)
{
	GstStateChangeReturn err;
	enum GstState state;

	if (!manager)
		return -EINVAL;

	err = gst_element_get_state(manager->play, &state, NULL, 0);
	if (err == GST_STATE_CHANGE_FAILURE)
		return -EINVAL;

	if (!statep)
		return 0;

	switch (state) {
	case GST_STATE_PLAYING:
		*statep = SOUND_MANAGER_PLAYING;
		break;
	case GST_STATE_PAUSED:
		*statep = SOUND_MANAGER_PAUSED;
		break;
	default:
		*statep = SOUND_MANAGER_STOPPED;
	}

	return 0;
}
