/*
 * Copyright (C) 2014 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <errno.h>
#include <math.h>
#include <limits.h>

#include "javascript.h"

#define MEDIA_PLAYER_ENUM(v, n) { .value = MEDIA_PLAYER_##v, .name = n }
#define MEDIA_PLAYER_SPU_MIN -1
#define MEDIA_PLAYER_SPU_MAX 0x1FFF
#define MEDIA_PLAYER_PID_MIN -1
#define MEDIA_PLAYER_PID_MAX 0x1FFF

#define MEDIA_PLAYER_TELETEXT_MIN 0
#define MEDIA_PLAYER_TELETEXT_MAX 999

struct js_media_player {
	GSource source;
	struct media_player *player;
	JSObjectRef callback;
	JSContextRef context;
	JSObjectRef this;
	GList *events;
};

static const struct javascript_enum media_player_state_enum[] = {
	MEDIA_PLAYER_ENUM(STOPPED, "stop"),
	MEDIA_PLAYER_ENUM(PLAYING, "play"),
	MEDIA_PLAYER_ENUM(PAUSED,  "pause"),
	{}
};

static const struct javascript_enum media_player_es_action_enum[] = {
	MEDIA_PLAYER_ENUM(ES_ADDED,    "add"),
	MEDIA_PLAYER_ENUM(ES_DELETED,  "del"),
	MEDIA_PLAYER_ENUM(ES_SELECTED, "sel"),
	{}
};

static const struct javascript_enum media_player_es_type_enum[] = {
	MEDIA_PLAYER_ENUM(ES_UNKNOWN, "unknown"),
	MEDIA_PLAYER_ENUM(ES_AUDIO,   "audio"),
	MEDIA_PLAYER_ENUM(ES_VIDEO,   "video"),
	MEDIA_PLAYER_ENUM(ES_TEXT,    "text"),
	{}
};

struct media_player_es_event {
	enum media_player_es_action action;
	enum media_player_es_type type;
	int pid;
};

void js_media_player_es_changed_cb(void *data,
		enum media_player_es_action action,
		enum media_player_es_type type, int pid)
{
	struct js_media_player *priv = (struct js_media_player *)data;
	struct media_player_es_event *event;

	if (!priv)
		return;

	event = g_new0(struct media_player_es_event, 1);
	if (!event)
		return;

	event->action = action;
	event->type = type;
	event->pid = pid;
	priv->events = g_list_append(priv->events, event);
}

static gboolean media_player_source_prepare(GSource *source, gint *timeout)
{
	if (timeout)
		*timeout = 100;

	return FALSE;
}

static gboolean media_player_source_check(GSource *source)
{
	struct js_media_player *priv = (struct js_media_player *)source;
	if (priv && g_list_first(priv->events))
		return TRUE;

	return FALSE;
}

static void media_player_send_es_event(struct js_media_player *priv,
		struct media_player_es_event *event)
{
	JSValueRef exception = NULL;
	JSValueRef args[3];

	args[0] = javascript_enum_to_string(priv->context,
			media_player_es_action_enum, event->action,
			&exception);
	args[1] = javascript_enum_to_string(priv->context,
			media_player_es_type_enum, event->type,
			&exception);
	args[2] = JSValueMakeNumber(priv->context, event->pid);
	(void)JSObjectCallAsFunction(priv->context, priv->callback,
			priv->this, G_N_ELEMENTS(args), args, &exception);
	if (exception)
		g_warning("%s: exception in es changed callback", __func__);
}

static gboolean media_player_source_dispatch(GSource *source, GSourceFunc callback,
		gpointer user_data)
{
	struct js_media_player *priv = (struct js_media_player *)source;
	GList *node = priv ? g_list_first(priv->events) : NULL;
	struct media_player_es_event *event = node ? node->data : NULL;

	while (event) {
		if (priv->context && priv->callback)
			media_player_send_es_event(priv, event);
		priv->events = g_list_remove(priv->events, event);
		g_free(event);
		node = g_list_first(priv->events);
		event = node ? node->data : NULL;
	}

	if (callback)
		return callback(user_data);

	return TRUE;
}

static void media_player_source_finalize(GSource *source)
{
	struct js_media_player *priv = (struct js_media_player *)source;

	g_list_free_full(priv->events, g_free);
}

static GSourceFuncs media_player_source_funcs = {
	.prepare = media_player_source_prepare,
	.check = media_player_source_check,
	.dispatch = media_player_source_dispatch,
	.finalize = media_player_source_finalize,
};

static JSValueRef js_media_player_get_uri(JSContextRef context,
		JSObjectRef object, JSStringRef name, JSValueRef *exception)
{
	struct js_media_player *priv = JSObjectGetPrivate(object);
	JSValueRef value;
	char *uri = NULL;
	int err;

	if (!priv) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	err = media_player_get_uri(priv->player, &uri);
	if (err) {
		javascript_set_exception_text(context, exception,
			"failed to get player uri");
		return NULL;
	}

	if (uri == NULL)
		return JSValueMakeNull(context);

	value = javascript_make_string(context, uri, exception);
	g_free(uri);

	return value;
}

static bool js_media_player_set_uri(JSContextRef context,
		JSObjectRef object, JSStringRef name, JSValueRef value,
		JSValueRef *exception)
{
	struct js_media_player *priv = JSObjectGetPrivate(object);
	char *uri;
	int err;

	if (!priv) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return false;
	}

	uri = javascript_get_string(context, value, exception);
	if (!uri)
		return false;

	err = media_player_set_uri(priv->player, uri);
	if (err)
		javascript_set_exception_text(context, exception,
			"failed to set uri");

	g_free(uri);
	return err == 0;
}

static JSValueRef js_media_player_get_duration(JSContextRef context,
		JSObjectRef object, JSStringRef name, JSValueRef *exception)
{
	struct js_media_player *priv = JSObjectGetPrivate(object);
	unsigned long duration;
	int err;

	if (!priv) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	err = media_player_get_duration(priv->player, &duration);
	if (err) {
		javascript_set_exception_text(context, exception,
			"failed to get media duration");
		return NULL;
	}

	return JSValueMakeNumber(context, duration);
}

static JSValueRef js_media_player_get_position(JSContextRef context,
		JSObjectRef object, JSStringRef name, JSValueRef *exception)
{
	struct js_media_player *priv = JSObjectGetPrivate(object);
	unsigned long position;
	int err;

	if (!priv) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	err = media_player_get_position(priv->player, &position);
	if (err) {
		javascript_set_exception_text(context, exception,
			"failed to get player position");
		return NULL;
	}

	return JSValueMakeNumber(context, position);
}

static bool js_media_player_set_position(JSContextRef context,
		JSObjectRef object, JSStringRef name, JSValueRef value,
		JSValueRef *exception)
{
	struct js_media_player *priv = JSObjectGetPrivate(object);
	double dval;
	int err;

	if (!priv) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	dval = JSValueToNumber(context, value, exception);
	if (isnan(dval))
		return false;

	if (dval < 0) {
		javascript_set_exception_text(context, exception,
			"position must be positive");
		return false;
	}

	err = media_player_set_position(priv->player, dval);
	if (err)
		javascript_set_exception_text(context, exception,
			"failed to set player position");

	return err == 0;
}

static JSValueRef js_media_player_get_state(JSContextRef context,
		JSObjectRef object, JSStringRef name, JSValueRef *exception)
{
	struct js_media_player *priv = JSObjectGetPrivate(object);
	enum media_player_state state;
	int err;

	if (!priv) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	err = media_player_get_state(priv->player, &state);
	if (err) {
		javascript_set_exception_text(context, exception,
			"failed to get player state");
		return NULL;
	}

	return javascript_enum_to_string(
		context, media_player_state_enum, state, exception);
}

static bool js_media_player_set_state(JSContextRef context,
		JSObjectRef object, JSStringRef name, JSValueRef value,
		JSValueRef *exception)
{
	struct js_media_player *priv = JSObjectGetPrivate(object);
	enum media_player_state current_state, next_state;
	int err = 0;

	if (!priv) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return false;
	}

	err = media_player_get_state(priv->player, &current_state);
	if (err) {
		javascript_set_exception_text(context, exception,
			"failed to get player state");
		return false;
	}

	err = javascript_enum_from_string(
		context, media_player_state_enum,
		value, (int*)&next_state, exception);
	if (err)
		return false;

	if (next_state == current_state)
		return true;

	switch (next_state) {
	case MEDIA_PLAYER_STOPPED:
		err = media_player_stop(priv->player);
		break;
	case MEDIA_PLAYER_PAUSED:
		err = media_player_pause(priv->player);
		break;
	case MEDIA_PLAYER_PLAYING:
		if (current_state == MEDIA_PLAYER_PAUSED)
			err = media_player_resume(priv->player);
		else
			err = media_player_play(priv->player);
		break;
	}

	if (err != 0) {
		char *name = javascript_get_string(context, value, NULL);
		javascript_set_exception_text(context, exception,
			"failed to set player state to %s", name);
		g_free(name);
	}

	return err == 0;
}

static JSValueRef js_media_player_get_mute(JSContextRef context,
		JSObjectRef object, JSStringRef name, JSValueRef *exception)
{
	struct js_media_player *priv = JSObjectGetPrivate(object);
	bool mute;
	int err;

	if (!priv) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	err = media_player_get_mute(priv->player, &mute);
	if (err) {
		javascript_set_exception_text(context, exception,
			"failed to get mute");
		return NULL;
	}

	return JSValueMakeBoolean(context, mute);
}

static bool js_media_player_set_mute(JSContextRef context,
		JSObjectRef object, JSStringRef name, JSValueRef value,
		JSValueRef *exception)
{
	struct js_media_player *priv = JSObjectGetPrivate(object);
	bool mute = JSValueToBoolean(context, value);
	int err;

	if (!priv) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return false;
	}

	err = media_player_set_mute(priv->player, mute);
	if (err) {
		javascript_set_exception_text(context, exception,
			"failed to set mute");
		return false;
	}

	return err == 0;
}

static JSValueRef js_media_player_get_audio_track(JSContextRef context,
		JSObjectRef object, JSStringRef name, JSValueRef *exception)
{
	struct js_media_player *priv = JSObjectGetPrivate(object);
	int err, pid;

	if (!priv) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	err = media_player_get_audio_track(priv->player, &pid);
	if (err) {
		javascript_set_exception_text(context, exception,
			"failed to get audio track");
		return NULL;
	}

	return JSValueMakeNumber(context, pid);
}

static bool js_media_player_set_audio_track(JSContextRef context,
		JSObjectRef object, JSStringRef name, JSValueRef value,
		JSValueRef *exception)
{
	struct js_media_player *priv = JSObjectGetPrivate(object);
	int err, pid;

	if (!priv) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return false;
	}

	err = javascript_int_from_number(context, value, MEDIA_PLAYER_PID_MIN,
			MEDIA_PLAYER_PID_MAX, &pid, exception);
	if (err)
		return false;

	err = media_player_set_audio_track(priv->player, pid);
	if (err) {
		javascript_set_exception_text(context, exception,
			"failed to set audio track");
		return false;
	}

	return true;
}

static JSValueRef js_media_player_get_audio_track_count(JSContextRef context,
		JSObjectRef object, JSStringRef name, JSValueRef *exception)
{
	struct js_media_player *priv = JSObjectGetPrivate(object);
	int err, count;

	if (!priv) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	err = media_player_get_audio_track_count(priv->player, &count);
	if (err) {
		javascript_set_exception_text(context, exception,
			"failed to get audio track count");
		return NULL;
	}

	return JSValueMakeNumber(context, count);
}

static JSValueRef js_media_player_get_subtitle(JSContextRef context,
		JSObjectRef object, JSStringRef name, JSValueRef *exception)
{
	struct js_media_player *priv = JSObjectGetPrivate(object);
	int err, pid;

	if (!priv) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	err = media_player_get_spu(priv->player, &pid);
	if (err) {
		javascript_set_exception_text(context, exception,
			"failed to get subtitle");
		return NULL;
	}

	return JSValueMakeNumber(context, pid);
}

static bool js_media_player_set_subtitle(JSContextRef context,
		JSObjectRef object, JSStringRef name, JSValueRef value,
		JSValueRef *exception)
{
	struct js_media_player *priv = JSObjectGetPrivate(object);
	int err, pid;

	if (!priv) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return false;
	}

	err = javascript_int_from_number(context, value, MEDIA_PLAYER_SPU_MIN,
			MEDIA_PLAYER_SPU_MAX, &pid, exception);
	if (err)
		return false;

	err = media_player_set_spu(priv->player, pid);
	if (err) {
		javascript_set_exception_text(context, exception,
			"failed to set subtitle");
		return false;
	}

	return true;
}

static JSValueRef js_media_player_get_subtitle_count(JSContextRef context,
		JSObjectRef object, JSStringRef name, JSValueRef *exception)
{
	struct js_media_player *priv = JSObjectGetPrivate(object);
	int err, count;

	if (!priv) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	err = media_player_get_spu_count(priv->player, &count);
	if (err) {
		javascript_set_exception_text(context, exception,
			"failed to get subtitle count");
		return NULL;
	}

	return JSValueMakeNumber(context, count);
}

static JSValueRef js_media_player_get_teletext(JSContextRef context,
		JSObjectRef object, JSStringRef name, JSValueRef *exception)
{
	struct js_media_player *priv = JSObjectGetPrivate(object);
	int err, page;

	if (!priv) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	err = media_player_get_teletext(priv->player, &page);
	if (err) {
		javascript_set_exception_text(context, exception,
			"failed to get teletext");
		return NULL;
	}

	return JSValueMakeNumber(context, page);
}

static bool js_media_player_set_teletext(JSContextRef context,
		JSObjectRef object, JSStringRef name, JSValueRef value,
		JSValueRef *exception)
{
	struct js_media_player *priv = JSObjectGetPrivate(object);
	int err, page;

	if (!priv) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return false;
	}

	err = javascript_int_from_number(context, value,
			MEDIA_PLAYER_TELETEXT_MIN, MEDIA_PLAYER_TELETEXT_MAX,
			&page, exception);
	if (err)
		return false;

	err = media_player_set_teletext(priv->player, page);
	if (err) {
		javascript_set_exception_text(context, exception,
			"failed to set teletext");
		return false;
	}

	return true;
}

static JSValueRef js_media_player_get_on_es_changed(JSContextRef context,
		JSObjectRef object, JSStringRef name, JSValueRef *exception)
{
	struct js_media_player *priv = JSObjectGetPrivate(object);

	if (!priv) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	return priv->callback;
}

static bool js_media_player_set_on_es_changed(JSContextRef context, JSObjectRef object,
		JSStringRef name, JSValueRef value, JSValueRef *exception)
{
	struct js_media_player *priv = JSObjectGetPrivate(object);
	int err;

	if (!priv) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return false;
	}

	if (priv->callback)
		JSValueUnprotect(context, priv->callback);

	if (JSValueIsNull(context, value)) {
		media_player_set_es_changed_callback(priv->player, NULL, priv);
		priv->callback = NULL;
		return true;
	}

	priv->callback = JSValueToObject(context, value, exception);
	if (!priv->callback) {
		javascript_set_exception_text(context, exception,
			"failed to set on es changed");
		return false;
	}
	JSValueProtect(context, priv->callback);

	err = media_player_set_es_changed_callback(priv->player,
			js_media_player_es_changed_cb, priv);
	if (err) {
		javascript_set_exception_text(context, exception,
			"failed to set es changed callback");
		return false;
	}

	return true;
}

static const JSStaticValue media_player_properties[] = {
	{
		.name = "uri",
		.getProperty = js_media_player_get_uri,
		.setProperty = js_media_player_set_uri,
		.attributes = kJSPropertyAttributeDontDelete,
	},
	{
		.name = "duration",
		.getProperty = js_media_player_get_duration,
		.attributes = kJSPropertyAttributeReadOnly |
			kJSPropertyAttributeDontDelete,
	},
	{
		.name = "position",
		.getProperty = js_media_player_get_position,
		.setProperty = js_media_player_set_position,
		.attributes = kJSPropertyAttributeDontDelete,
	},
	{
		.name = "state",
		.getProperty = js_media_player_get_state,
		.setProperty = js_media_player_set_state,
		.attributes = kJSPropertyAttributeDontDelete,
	},
	{
		.name = "mute",
		.getProperty = js_media_player_get_mute,
		.setProperty = js_media_player_set_mute,
		.attributes = kJSPropertyAttributeDontDelete,
	},
	{
		.name = "audioTrack",
		.getProperty = js_media_player_get_audio_track,
		.setProperty = js_media_player_set_audio_track,
		.attributes = kJSPropertyAttributeDontDelete,
	},
	{
		.name = "audioTrackCount",
		.getProperty = js_media_player_get_audio_track_count,
		.attributes = kJSPropertyAttributeReadOnly |
			kJSPropertyAttributeDontDelete,
	},
	{
		.name = "subtitle",
		.getProperty = js_media_player_get_subtitle,
		.setProperty = js_media_player_set_subtitle,
		.attributes = kJSPropertyAttributeDontDelete,
	},
	{
		.name = "subtitleCount",
		.getProperty = js_media_player_get_subtitle_count,
		.attributes = kJSPropertyAttributeReadOnly |
			kJSPropertyAttributeDontDelete,
	},
	{
		.name = "teletext",
		.getProperty = js_media_player_get_teletext,
		.setProperty = js_media_player_set_teletext,
		.attributes = kJSPropertyAttributeDontDelete,
	},
	{
		.name = "onEsChanged",
		.getProperty = js_media_player_get_on_es_changed,
		.setProperty = js_media_player_set_on_es_changed,
		.attributes = kJSPropertyAttributeNone,
	},
	{}
};

static JSValueRef js_media_player_set_crop(JSContextRef context,
		JSObjectRef function, JSObjectRef object,
		size_t argc, const JSValueRef argv[],
		JSValueRef *exception)
{
	struct js_media_player *priv = JSObjectGetPrivate(object);
	unsigned int args[4];
	double dval;
	int i, err;

	if (!priv) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return false;
	}

	if (argc != 4) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_ARG_COUNT);
		return NULL;
	}

	for (i = 0; i < 4 ; i++) {
		dval = JSValueToNumber(context, argv[i], exception);
		if (isnan(dval))
			return NULL;
		if (dval < 0)
			args[i] = 0;
		else if (dval > UINT_MAX)
			args[i] = UINT_MAX;
		else
			args[i] = dval;
	}

	err = media_player_set_crop(
		priv->player, args[0], args[1], args[2], args[3]);
	if (err) {
		javascript_set_exception_text(context, exception,
			"failed to set crop");
		return NULL;
	}

	return NULL;
}

static JSValueRef js_media_player_set_window(JSContextRef context,
		JSObjectRef function, JSObjectRef object,
		size_t argc, const JSValueRef argv[],
		JSValueRef *exception)
{
	struct js_media_player *priv = JSObjectGetPrivate(object);
	unsigned int args[4];
	double dval;
	int i, err;

	if (!priv) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return false;
	}

	if (argc != 4) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_ARG_COUNT);
		return NULL;
	}

	for (i = 0; i < 4 ; i++) {
		dval = JSValueToNumber(context, argv[i], exception);
		if (isnan(dval))
			return NULL;
		if (dval < 0)
			args[i] = 0;
		else if (dval > UINT_MAX)
			args[i] = UINT_MAX;
		else
			args[i] = dval;
	}

	err = media_player_set_output_window(
		priv->player, args[0], args[1], args[2], args[3]);
	if (err)
		javascript_set_exception_text(context, exception,
			"failed to set output window");

	return NULL;
}

static JSValueRef js_media_player_get_audio_track_pid(JSContextRef context,
		JSObjectRef function, JSObjectRef object,
		size_t argc, const JSValueRef argv[],
		JSValueRef *exception)
{
	struct js_media_player *priv = JSObjectGetPrivate(object);
	int err, pos, pid;

	if (!priv) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	switch (argc) {
	case 1: /* Index of audio track */
		err = javascript_int_from_number(context, argv[0],
				MEDIA_PLAYER_PID_MIN, MEDIA_PLAYER_PID_MAX,
				&pos, exception);
		if (err)
			return NULL;
		break;
	default:
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_ARG_COUNT);
		return NULL;
	}

	err = media_player_get_audio_track_pid(priv->player, pos, &pid);
	if (err) {
		javascript_set_exception_text(context, exception,
			"failed to get audio track name");
		return NULL;
	}

	return JSValueMakeNumber(context, pid);
}

static JSValueRef js_media_player_get_audio_track_name(JSContextRef context,
		JSObjectRef function, JSObjectRef object,
		size_t argc, const JSValueRef argv[],
		JSValueRef *exception)
{
	struct js_media_player *priv = JSObjectGetPrivate(object);
	char *name = NULL;
	JSValueRef ret;
	int err, pid;

	if (!priv) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	switch (argc) {
	case 0: /* Current audio track pid */
		err = media_player_get_audio_track(priv->player, &pid);
		if (err) {
			javascript_set_exception_text(context, exception,
				"failed to get current audio track");
			return NULL;
		}
		break;
	case 1: /* Audio track pid */
		err = javascript_int_from_number(context, argv[0],
				MEDIA_PLAYER_PID_MIN, MEDIA_PLAYER_PID_MAX,
				&pid, exception);
		if (err)
			return NULL;
		break;
	default:
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_ARG_COUNT);
		return NULL;
	}

	err = media_player_get_audio_track_name(priv->player, pid, &name);
	if (err) {
		javascript_set_exception_text(context, exception,
			"failed to get audio track name");
		return NULL;
	}

	ret = javascript_make_string(context, name, exception);
	g_free(name);

	return ret;
}

static JSValueRef js_media_player_get_subtitle_pid(JSContextRef context,
		JSObjectRef function, JSObjectRef object,
		size_t argc, const JSValueRef argv[],
		JSValueRef *exception)
{
	struct js_media_player *priv = JSObjectGetPrivate(object);
	int err, pos, pid;

	if (!priv) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	switch (argc) {
	case 1: /* Index of subtitle */
		err = javascript_int_from_number(context, argv[0],
				MEDIA_PLAYER_SPU_MIN, MEDIA_PLAYER_SPU_MAX,
				&pos, exception);
		if (err)
			return NULL;
		break;
	default:
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_ARG_COUNT);
		return NULL;
	}

	err = media_player_get_spu_pid(priv->player, pos, &pid);
	if (err) {
		javascript_set_exception_text(context, exception,
			"failed to get subtitle name");
		return NULL;
	}

	return JSValueMakeNumber(context, pid);
}

static JSValueRef js_media_player_get_subtitle_name(JSContextRef context,
		JSObjectRef function, JSObjectRef object,
		size_t argc, const JSValueRef argv[],
		JSValueRef *exception)
{
	struct js_media_player *priv = JSObjectGetPrivate(object);
	char *name = NULL;
	JSValueRef ret;
	int err, pid;

	if (!priv) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	switch (argc) {
	case 0: /* Current subtitle pid */
		err = media_player_get_spu(priv->player, &pid);
		if (err) {
			javascript_set_exception_text(context, exception,
				"failed to get current subtitle");
			return NULL;
		}
		break;
	case 1: /* Subtitle pid */
		err = javascript_int_from_number(context, argv[0],
				MEDIA_PLAYER_SPU_MIN, MEDIA_PLAYER_SPU_MAX,
				&pid, exception);
		if (err)
			return NULL;
		break;
	default:
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_ARG_COUNT);
		return NULL;
	}

	err = media_player_get_spu_name(priv->player, pid, &name);
	if (err) {
		javascript_set_exception_text(context, exception,
			"failed to get subtitle name");
		return NULL;
	}

	ret = javascript_make_string(context, name, exception);
	g_free(name);

	return ret;
}

static JSValueRef js_media_player_toggle_teletext_transparent(
		JSContextRef context, JSObjectRef function, JSObjectRef object,
		size_t argc, const JSValueRef argv[],
		JSValueRef *exception)
{
	struct js_media_player *priv = JSObjectGetPrivate(object);
	int err;

	if (!priv) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	if (argc) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_ARG_COUNT);
		return NULL;
	}

	err = media_player_toggle_teletext_transparent(priv->player);
	if (err) {
		javascript_set_exception_text(context, exception,
			"failed to toggle teletext transparent");
		return NULL;
	}

	return NULL;
}

static const JSStaticFunction media_player_functions[] = {
	{
		.name = "setCrop",
		.callAsFunction = js_media_player_set_crop,
		.attributes = kJSPropertyAttributeDontDelete,
	},
	{
		.name = "setWindow",
		.callAsFunction = js_media_player_set_window,
		.attributes = kJSPropertyAttributeDontDelete,
	},
	{
		.name = "getAudioTrackPid",
		.callAsFunction = js_media_player_get_audio_track_pid,
		.attributes = kJSPropertyAttributeDontDelete,
	},
	{
		.name = "getAudioTrackName",
		.callAsFunction = js_media_player_get_audio_track_name,
		.attributes = kJSPropertyAttributeDontDelete,
	},
	{
		.name = "getSubtitlePid",
		.callAsFunction = js_media_player_get_subtitle_pid,
		.attributes = kJSPropertyAttributeDontDelete,
	},
	{
		.name = "getSubtitleName",
		.callAsFunction = js_media_player_get_subtitle_name,
		.attributes = kJSPropertyAttributeDontDelete,
	},
	{
		.name = "toggleTeletextTransparent",
		.callAsFunction = js_media_player_toggle_teletext_transparent,
		.attributes = kJSPropertyAttributeDontDelete,
	},
	{}
};

static void media_player_initialize(JSContextRef context, JSObjectRef object)
{
	struct js_media_player *priv = JSObjectGetPrivate(object);

	priv->this = object;
}

static void media_player_finalize(JSObjectRef object)
{
	struct js_media_player *priv = JSObjectGetPrivate(object);

	if (priv->callback) {
		media_player_set_es_changed_callback(priv->player,
				NULL, priv);
		JSValueUnprotect(priv->context, priv->callback);
	}
	g_source_destroy(&priv->source);
}

static const JSClassDefinition media_player_classdef = {
	.className = "MediaPlayer",
	.initialize = media_player_initialize,
	.finalize = media_player_finalize,
	.staticValues = media_player_properties,
	.staticFunctions = media_player_functions,
};

static JSObjectRef javascript_media_player_create(
	JSContextRef js, JSClassRef class,
	struct javascript_userdata *user_data)
{
	struct js_media_player *priv;
	GSource *source;

	source = g_source_new(&media_player_source_funcs, sizeof(*priv));
	priv = (struct js_media_player *)source;
	if (!priv)
		return NULL;

	priv->player = remote_control_get_media_player(user_data->rcd->rc);
	if (!priv->player)
		goto cleanup;

	priv->callback = NULL;
	priv->context = js;

	g_source_attach(source, g_main_loop_get_context(user_data->loop));
	return JSObjectMake(js, class, source);
cleanup:
	g_source_destroy(source);
	return NULL;
}

struct javascript_module javascript_media_player = {
	.classdef = &media_player_classdef,
	.create = javascript_media_player_create,
};
