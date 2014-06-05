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
			"object not valid, context switched?");
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
			"object not valid, context switched?");
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
			"object not valid, context switched?");
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
			"object not valid, context switched?");
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
			"object not valid, context switched?");
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
			"object not valid, context switched?");
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

	return err == 0;
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
			"object not valid, context switched?");
		return false;
	}

	if (argc != 4) {
		javascript_set_exception_text(context, exception,
			"invalid arguments count");
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
			"object not valid, context switched?");
		return false;
	}

	if (argc != 4) {
		javascript_set_exception_text(context, exception,
			"invalid arguments count");
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
