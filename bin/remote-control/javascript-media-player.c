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

#define MEDIA_PLAYER_STATE(v, n) { .value = MEDIA_PLAYER_##v, .name = n }
#define MEDIA_PLAYER_SPU_MIN -1
#define MEDIA_PLAYER_SPU_MAX 0x1FFF

#define MEDIA_PLAYER_TELETEXT_MIN 0
#define MEDIA_PLAYER_TELETEXT_MAX 999

static const struct javascript_enum media_player_state_enum[] = {
	MEDIA_PLAYER_STATE(STOPPED,	"stop"),
	MEDIA_PLAYER_STATE(PLAYING,	"play"),
	MEDIA_PLAYER_STATE(PAUSED,	"pause"),
	{}
};

static JSValueRef js_media_player_get_uri(JSContextRef context,
		JSObjectRef object, JSStringRef name, JSValueRef *exception)
{
	struct media_player *player = JSObjectGetPrivate(object);
	JSValueRef value;
	char *uri = NULL;
	int err;

	if (!player) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	err = media_player_get_uri(player, &uri);
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
	struct media_player *player = JSObjectGetPrivate(object);
	char *uri;
	int err;

	if (!player) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return false;
	}

	uri = javascript_get_string(context, value, exception);
	if (!uri)
		return false;

	err = media_player_set_uri(player, uri);
	if (err)
		javascript_set_exception_text(context, exception,
			"failed to set uri");

	g_free(uri);
	return err == 0;
}

static JSValueRef js_media_player_get_duration(JSContextRef context,
		JSObjectRef object, JSStringRef name, JSValueRef *exception)
{
	struct media_player *player = JSObjectGetPrivate(object);
	unsigned long duration;
	int err;

	if (!player) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	err = media_player_get_duration(player, &duration);
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
	struct media_player *player = JSObjectGetPrivate(object);
	unsigned long position;
	int err;

	if (!player) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	err = media_player_get_position(player, &position);
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
	struct media_player *player = JSObjectGetPrivate(object);
	double dval;
	int err;

	dval = JSValueToNumber(context, value, exception);
	if (isnan(dval))
		return false;

	if (dval < 0) {
		javascript_set_exception_text(context, exception,
			"position must be positive");
		return false;
	}

	err = media_player_set_position(player, dval);
	if (err)
		javascript_set_exception_text(context, exception,
			"failed to set player position");

	return err == 0;
}

static JSValueRef js_media_player_get_state(JSContextRef context,
		JSObjectRef object, JSStringRef name, JSValueRef *exception)
{
	struct media_player *player = JSObjectGetPrivate(object);
	enum media_player_state state;
	int err;

	if (!player) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	err = media_player_get_state(player, &state);
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
	struct media_player *player = JSObjectGetPrivate(object);
	enum media_player_state current_state, next_state;
	int err = 0;

	if (!player) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return false;
	}

	err = media_player_get_state(player, &current_state);
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
		err = media_player_stop(player);
		break;
	case MEDIA_PLAYER_PAUSED:
		err = media_player_pause(player);
		break;
	case MEDIA_PLAYER_PLAYING:
		if (current_state == MEDIA_PLAYER_PAUSED)
			err = media_player_resume(player);
		else
			err = media_player_play(player);
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
	struct media_player *player = JSObjectGetPrivate(object);
	bool mute;
	int err;

	if (!player) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	err = media_player_get_mute(player, &mute);
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
	struct media_player *player = JSObjectGetPrivate(object);
	bool mute = JSValueToBoolean(context, value);
	int err;

	if (!player) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return false;
	}

	err = media_player_set_mute(player, mute);
	if (err) {
		javascript_set_exception_text(context, exception,
			"failed to set mute");
		return false;
	}

	return err == 0;
}

static JSValueRef js_media_player_get_subtitle(JSContextRef context,
		JSObjectRef object, JSStringRef name, JSValueRef *exception)
{
	struct media_player *player = JSObjectGetPrivate(object);
	int err, pid;

	if (!player) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	err = media_player_get_spu(player, &pid);
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
	struct media_player *player = JSObjectGetPrivate(object);
	int err, pid;

	err = javascript_int_from_number(context, value, MEDIA_PLAYER_SPU_MIN,
			MEDIA_PLAYER_SPU_MAX, &pid, exception);
	if (err)
		return false;

	if (!player) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return false;
	}

	err = media_player_set_spu(player, pid);
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
	struct media_player *player = JSObjectGetPrivate(object);
	int err, count;

	if (!player) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	err = media_player_get_spu_count(player, &count);
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
	struct media_player *player = JSObjectGetPrivate(object);
	int err, page;

	if (!player) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	err = media_player_get_teletext(player, &page);
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
	struct media_player *player = JSObjectGetPrivate(object);
	int err, page;

	err = javascript_int_from_number(context, value,
			MEDIA_PLAYER_TELETEXT_MIN, MEDIA_PLAYER_TELETEXT_MAX,
			&page, exception);
	if (err)
		return false;

	if (!player) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return false;
	}

	err = media_player_set_teletext(player, page);
	if (err) {
		javascript_set_exception_text(context, exception,
			"failed to set teletext");
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
	{}
};

static JSValueRef js_media_player_set_crop(JSContextRef context,
		JSObjectRef function, JSObjectRef object,
		size_t argc, const JSValueRef argv[],
		JSValueRef *exception)
{
	struct media_player *player = JSObjectGetPrivate(object);
	unsigned int args[4];
	double dval;
	int i, err;

	if (!player) {
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
		player, args[0], args[1], args[2], args[3]);
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
	struct media_player *player = JSObjectGetPrivate(object);
	unsigned int args[4];
	double dval;
	int i, err;

	if (!player) {
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
		player, args[0], args[1], args[2], args[3]);
	if (err)
		javascript_set_exception_text(context, exception,
			"failed to set output window");

	return NULL;
}

static JSValueRef js_media_player_get_subtitle_pid(JSContextRef context,
		JSObjectRef function, JSObjectRef object,
		size_t argc, const JSValueRef argv[],
		JSValueRef *exception)
{
	struct media_player *player = JSObjectGetPrivate(object);
	int err, pos, pid;

	if (!player) {
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

	err = media_player_get_spu_pid(player, pos, &pid);
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
	struct media_player *player = JSObjectGetPrivate(object);
	char *name = NULL;
	JSValueRef ret;
	int err, pid;

	if (!player) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	switch (argc) {
	case 0: /* Current subtitle pid */
		err = media_player_get_spu(player, &pid);
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

	err = media_player_get_spu_name(player, pid, &name);
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
	struct media_player *player = JSObjectGetPrivate(object);
	int err;

	if (!player) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	if (argc) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_ARG_COUNT);
		return NULL;
	}

	err = media_player_toggle_teletext_transparent(player);
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


static const JSClassDefinition media_player_classdef = {
	.className = "MediaPlayer",
	.staticValues = media_player_properties,
	.staticFunctions = media_player_functions,
};

static JSObjectRef javascript_media_player_create(
	JSContextRef js, JSClassRef class,
	struct javascript_userdata *user_data)
{
	struct media_player *player;

	player = remote_control_get_media_player(user_data->rcd->rc);
	if (!player)
		return NULL;

	return JSObjectMake(js, class, player);
}

struct javascript_module javascript_media_player = {
	.classdef = &media_player_classdef,
	.create = javascript_media_player_create,
};
