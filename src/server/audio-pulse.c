/*
 * Copyright (C) 2013 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>

#include <alsa/use-case.h>

#include <pulse/pulseaudio.h>
#include <pulse/glib-mainloop.h>

#include "remote-control.h"


struct audio {
	pa_glib_mainloop *loop;
	pa_context *ctx;

	int source;
	int sink;

	enum audio_state state;
};

static int pulse_connect(struct audio *self);
static void pulse_disconnect(struct audio *self);

static void resume_complete(pa_context *ctx, int success, void *userdata)
{
	/* 0=resume, 1=suspend */
	if (!success) {
		g_warning("alsa-pulse: Failure during resume: %s",
			pa_strerror(pa_context_errno(ctx)));
	}
}

static void sink_info_cb(pa_context *ctx, const pa_sink_info *info, int eol,
			 void *userdata)
{
	struct audio *self = userdata;
	pa_operation *op;

	g_assert(self);
	g_assert(self->ctx == ctx);

	if (eol || pa_context_errno(self->ctx) == PA_ERR_NOENTITY)
		return;

	self->sink = info->index;

	op = pa_context_suspend_sink_by_index(ctx, info->index, 0,
					      resume_complete, self);
	pa_operation_unref(op);
}

static void source_info_cb(pa_context *ctx, const pa_source_info *info,
			   int eol, void *userdata)
{
	struct audio *self = userdata;
	pa_operation *op;

	g_assert(self);
	g_assert(self->ctx == ctx);

	if (eol || pa_context_errno(self->ctx) == PA_ERR_NOENTITY)
		return;

	self->source = info->index;

	op = pa_context_suspend_source_by_index(ctx, info->index, 0,
						resume_complete, self);
	pa_operation_unref(op);
}

static pa_card_info *pulse_update_config(struct audio *self)
{
	pa_operation *op;

	/* TODO: use pa_context_subscribe for hotplug support */
	op = pa_context_get_sink_info_list(self->ctx, sink_info_cb, self);
	if (op == NULL)
		g_warning("alsa-pulse: failed to query sink info");
	pa_operation_unref(op);

	op = pa_context_get_source_info_list(self->ctx, source_info_cb, self);
	if (op == NULL)
		g_warning("alsa-pulse: failed to query source info");
	pa_operation_unref(op);

	return NULL;
}

static inline gboolean pulse_reconnect(gpointer data)
{
	struct audio *self = (struct audio*)data;

	if (!pulse_connect(self))
		g_warning("alsa-pulse: reconnect failed");

	return false;
}

static void context_state_cb(pa_context *ctx, void *userdata)
{
	struct audio *self = userdata;

	g_assert(self);
	g_assert(self->ctx == ctx);

	switch (pa_context_get_state(self->ctx)) {
	case PA_CONTEXT_UNCONNECTED:
	case PA_CONTEXT_CONNECTING:
	case PA_CONTEXT_AUTHORIZING:
	case PA_CONTEXT_SETTING_NAME:
		/* ignore */
		break;

	case PA_CONTEXT_READY:
		g_debug("alsa-pulse: connection is ready");
		pulse_update_config(self);
		break;

	case PA_CONTEXT_FAILED:
		g_warning("alsa-pulse: connection lost");
		pulse_disconnect(self);
		g_timeout_add_seconds(1, pulse_reconnect, self);
		break;

	case PA_CONTEXT_TERMINATED:
		g_debug("alsa-pulse: connection terminated");
		pulse_disconnect(self);
		break;
	default:
		g_warning("alsa-pulse: %s: unexpected state", __func__);
		break;
	}
}

static void pulse_disconnect(struct audio *self)
{
	if (!self)
		return;

	if (self->ctx) {
		pa_context_disconnect(self->ctx);
		pa_context_unref(self->ctx);
		self->ctx = NULL;
	}

	if (self->loop) {
		pa_glib_mainloop_free(self->loop);
		self->loop = NULL;
	}
}

/* the return value is required for g_timer_add_seconds */
static gboolean pulse_connect(struct audio *self)
{
	pa_mainloop_api *api;
	pa_proplist *proplist;
	int err;

	self->loop = pa_glib_mainloop_new(g_main_context_default());
	g_assert(self->loop);

	api = pa_glib_mainloop_get_api(self->loop);
	g_assert(api);

	proplist = pa_proplist_new();
	g_assert(proplist);

	pa_proplist_sets(proplist, PA_PROP_APPLICATION_NAME,
			 "remote-control - pulseaudio backend");
	pa_proplist_sets(proplist, PA_PROP_APPLICATION_ID,
			 "de.avionic-design.remote-control");
	pa_proplist_sets(proplist, PA_PROP_APPLICATION_ICON_NAME,
			 "audio-card");
	pa_proplist_sets(proplist, PA_PROP_APPLICATION_VERSION,
			 PACKAGE_VERSION);

	self->ctx = pa_context_new_with_proplist(api, NULL, proplist);
	g_assert(self->ctx);
	pa_proplist_free(proplist);

	pa_context_set_state_callback(self->ctx, context_state_cb, self);

	err = pa_context_connect(self->ctx, g_getenv("PULSE_SERVER"),
				 PA_CONTEXT_NOFAIL, NULL);
	if (err < 0) {
		g_warning("alsa-pulse: unable to create pulseaudio context");
		goto free;
	}

	return true;

free:
	pulse_disconnect(self);
	return false;
}

int audio_create(struct audio **audiop, struct remote_control *rc,
		GKeyFile *config)
{
	struct audio *self;

	self = g_new0(struct audio, 1);
	if (!self)
		return -ENOMEM;

	if (!pulse_connect(self)) {
		g_warning("audio-pulse: failed to connect to pulse server");
		g_free(self);
		return -ECONNREFUSED;
	}

	*audiop = self;
	return 0;
}

int audio_free(struct audio *self)
{
	pulse_disconnect(self);
	g_free(self);
	return 0;
}

static void source_port_cb(pa_context *ctx, int success, void *userdata)
{
	g_debug("updated source port: %s", success ? "done" : "failed");
}

static void sink_port_cb(pa_context *ctx, int success, void *userdata)
{
	g_debug("updated sink port: %s", success ? "done" : "failed");
}

static void source_mute_cb(pa_context *ctx, int success, void *userdata)
{
	g_debug("unmuted source: %s", success ? "done" : "failed");
}

static void sink_mute_cb(pa_context *ctx, int success, void *userdata)
{
	g_debug("unmuted sink: %s", success ? "done" : "failed");
}

static const gchar *state_to_port(enum audio_state state)
{
	switch (state) {
	case AUDIO_STATE_HIFI_PLAYBACK_SPEAKER:
		return SND_USE_CASE_DEV_SPEAKER;
	case AUDIO_STATE_HIFI_PLAYBACK_HEADSET:
		return SND_USE_CASE_DEV_HEADPHONES;
	case AUDIO_STATE_VOICECALL_IP_HANDSET:
		return SND_USE_CASE_DEV_HANDSET;
	default:
		return SND_USE_CASE_DEV_NONE;
	}
}

int audio_set_state(struct audio *self, enum audio_state state)
{
	const gchar *port = state_to_port(state);
	pa_operation *op;
	gchar *pport;

	self->state = state;

	/* pulseaudio is prefixing the UCM device with [Out] */
	pport = g_strdup_printf("[Out] %s", port);
	g_debug("   setting port %s on sink %d", port, self->sink);
	op  = pa_context_set_sink_port_by_index(self->ctx, self->sink, pport,
						sink_port_cb, self);
	pa_operation_unref(op);
	g_free(pport);

	/* pulseaudio is prefixing the UCM device with [In] */
	pport = g_strdup_printf("[In] %s", port);
	g_debug("   setting port %s on source %d", port, self->source);
	op = pa_context_set_source_port_by_index(self->ctx, self->source,
						 pport, source_port_cb, self);
	pa_operation_unref(op);
	g_free(pport);

	g_debug("   unmute sink");
	op = pa_context_set_sink_mute_by_index(self->ctx, self->sink, false,
					       sink_mute_cb, self);
	pa_operation_unref(op);

	g_debug("   unmute source");
	op = pa_context_set_source_mute_by_index(self->ctx, self->source,
						 false, source_mute_cb, self);
	pa_operation_unref(op);
	return 0;
}

int audio_get_state(struct audio *self, enum audio_state *statep)
{
	*statep = self->state;
	return -EINVAL;
}

int audio_set_volume(struct audio *self, uint8_t volume)
{
	pa_volume_t cor = (PA_VOLUME_NORM / 255) * volume;
	pa_operation *op;
	pa_cvolume vol;
	int err = 0;

	pa_cvolume_init(&vol);
	pa_cvolume_set(&vol, 2, cor);

	op = pa_context_set_sink_volume_by_index(self->ctx,
						 self->sink,
						 &vol, NULL, NULL);
	if (!op) {
		g_warning("alsa-pulse: failed to set volume");
		err = -ENOSYS;
	}
	pa_operation_unref(op);

	return err;
}

int audio_get_volume(struct audio *self, uint8_t *volumep)
{
	pa_operation *op;
	int err;

	op = pa_context_get_sink_info_by_index(self->ctx, self->sink,
					       sink_info_cb, self);
	if (!op) {
		g_warning("alsa-pulse: failed to get volume");
		err = -ENOSYS;
	} else
		pa_operation_unref(op);

	return err;
}

int audio_set_speakers_enable(struct audio *self, bool enable)
{
	return -ENOSYS;
}

int audio_get_speakers_enable(struct audio *self, bool *enablep)
{
	return -ENOSYS;
}
