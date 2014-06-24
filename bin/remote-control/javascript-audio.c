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

#include "remote-control-data.h"
#include "remote-control.h"
#include "javascript.h"

struct js_audio_player {
	struct sound_manager *manager;
	char *uri;
};

#define SOUND_MANAGER_STATE(v, n) { .value = SOUND_MANAGER_##v, .name = n }

static const struct javascript_enum sound_manager_state_enum[] = {
	SOUND_MANAGER_STATE(STOPPED,	"stop"),
	SOUND_MANAGER_STATE(PLAYING,	"play"),
	SOUND_MANAGER_STATE(PAUSED,	"pause"),
	{}
};

#define AUDIO_STATE(v, n) { .value = AUDIO_STATE_##v, .name = n }

/* We expose the same names as in the JSON API */
static const struct javascript_enum audio_state_enum[] = {
	AUDIO_STATE(INACTIVE, 			"idle"),
	AUDIO_STATE(HIFI_PLAYBACK_SPEAKER,	"hifi-speaker"),
	AUDIO_STATE(HIFI_PLAYBACK_HEADSET,	"hifi-headset"),
	AUDIO_STATE(VOICECALL_HANDSET,		"call-handset"),
	AUDIO_STATE(VOICECALL_HEADSET,		"call-headset"),
	AUDIO_STATE(VOICECALL_SPEAKER,		"call-speaker"),
	AUDIO_STATE(VOICECALL_IP_HANDSET,	"voip-handset"),
	AUDIO_STATE(VOICECALL_IP_HEADSET,	"voip-headset"),
	AUDIO_STATE(VOICECALL_IP_SPEAKER,	"voip-speaker"),
	AUDIO_STATE(LINEIN_SPEAKER,		"linein-speaker"),
	AUDIO_STATE(LINEIN_HEADSET,		"linein-headset"),
	{}
};

static JSValueRef js_audio_player_get_uri(
	JSContextRef context, JSObjectRef object,
	JSStringRef name, JSValueRef *exception)
{
	struct js_audio_player *player = JSObjectGetPrivate(object);

	if (!player) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	if (!player->uri)
		return JSValueMakeNull(context);

	return javascript_make_string(context, player->uri, exception);
}

static bool js_audio_player_set_uri(
	JSContextRef context, JSObjectRef object,
	JSStringRef name, JSValueRef value,
	JSValueRef *exception)
{
	struct js_audio_player *player = JSObjectGetPrivate(object);

	if (!player) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return false;
	}

	if (player->uri) {
		g_free(player->uri);
		player->uri = NULL;
		sound_manager_stop(player->manager);
	}

	if (!JSValueIsNull(context, value))
		player->uri = javascript_get_string(
			context, value, exception);

	return true;
}

static JSValueRef js_audio_player_get_state(
	JSContextRef context, JSObjectRef object,
	JSStringRef name, JSValueRef *exception)
{
	struct js_audio_player *player = JSObjectGetPrivate(object);
	enum sound_manager_state state;
	int err;

	if (!player) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	err = sound_manager_get_state(player->manager, &state);
	if (err) {
		javascript_set_exception_text(context, exception,
			"failed to get player state");
		return NULL;
	}

	return javascript_enum_to_string(
		context, sound_manager_state_enum, state, exception);
}

static bool js_audio_player_set_state(
	JSContextRef context, JSObjectRef object,
	JSStringRef name, JSValueRef value,
	JSValueRef *exception)
{
	struct js_audio_player *player = JSObjectGetPrivate(object);
	enum sound_manager_state current_state, next_state;
	int err;

	if (!player) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return true;
	}

	err = sound_manager_get_state(player->manager, &current_state);
	if (err) {
		javascript_set_exception_text(context, exception,
			"failed to get player state");
		return true;
	}

	err = javascript_enum_from_string(
		context, sound_manager_state_enum,
		value, (int *)&next_state, exception);
	if (err)
		return true;

	if (next_state == current_state)
		return true;

	switch (next_state) {
	case SOUND_MANAGER_STOPPED:
		err = sound_manager_stop(player->manager);
		break;
	case SOUND_MANAGER_PAUSED:
		if (current_state == SOUND_MANAGER_PLAYING)
			err = sound_manager_pause(player->manager);
		break;
	case SOUND_MANAGER_PLAYING:
		if (current_state == SOUND_MANAGER_PAUSED)
			err = sound_manager_pause(player->manager);
		else
			err = sound_manager_play(player->manager, player->uri);
		break;
	default: /* Can't happen! */
		javascript_set_exception_text(context, exception,
			"can't switch to unknown state");
		return true;
	}

	if (err != 0) {
		char *name = javascript_get_string(context, value, NULL);
		javascript_set_exception_text(context, exception,
			"failed to set player state to %s", name);
		g_free(name);
	}

	return true;
}

static const JSStaticValue js_audio_player_properties[] = {
	{
		.name = "uri",
		.getProperty = js_audio_player_get_uri,
		.setProperty = js_audio_player_set_uri,
		.attributes = kJSPropertyAttributeDontDelete,
	},
	{
		.name = "state",
		.getProperty = js_audio_player_get_state,
		.setProperty = js_audio_player_set_state,
		.attributes = kJSPropertyAttributeDontDelete,
	},
	{}
};

static void js_audio_player_finalize(JSObjectRef object)
{
	struct js_audio_player *player = JSObjectGetPrivate(object);

	g_free(player);
}

static JSClassDefinition audio_player_classdef = {
	.className = "AudioPlayer",
	.staticValues = js_audio_player_properties,
	.finalize = js_audio_player_finalize,
};

static JSObjectRef javascript_audio_player_create(
	JSContextRef js, JSClassRef class,
	struct javascript_userdata *user_data)
{
	struct sound_manager *manager;
	struct js_audio_player *player;

	if (!user_data->rcd || !user_data->rcd->rc)
		return NULL;

	manager = remote_control_get_sound_manager(user_data->rcd->rc);
	if (!manager)
		return NULL;

	player = g_malloc0(sizeof(*player));
	if (!player)
		return NULL;
	player->manager = manager;

	return JSObjectMake(js, class, player);
}

struct javascript_module javascript_audio_player = {
	.classdef = &audio_player_classdef,
	/* This class object is instanced by the Audio class */
};

static bool js_audio_set_state(JSContextRef context, JSObjectRef object,
		JSStringRef name, JSValueRef value, JSValueRef *exception)
{
	struct audio *audio = JSObjectGetPrivate(object);
	enum audio_state state;
	int err;

	if (!audio) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return false;
	}

	err = javascript_enum_from_string(
		context, audio_state_enum, value, (int*)&state, exception);
	if (err)
		return false;

	err = audio_set_state(audio, state);
	if (err)
		javascript_set_exception_text(context, exception,
			"failed to set audio state");

	return err == 0;
}

static JSValueRef js_audio_get_state(JSContextRef context, JSObjectRef object,
		JSStringRef name, JSValueRef *exception)
{
	struct audio *audio = JSObjectGetPrivate(object);
	enum audio_state state;
	int err;

	if (!audio) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	err = audio_get_state(audio, &state);
	if (err) {
		javascript_set_exception_text(context, exception,
			"failed to get audio state");
		return NULL;
	}

	return javascript_enum_to_string(
		context, audio_state_enum, state, exception);
}

static bool js_audio_set_volume(JSContextRef context, JSObjectRef object,
		JSStringRef name, JSValueRef value, JSValueRef *exception)
{
	struct audio *audio = JSObjectGetPrivate(object);
	int volume;
	int err;

	if (!audio) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return false;
	}

	err = javascript_int_from_unit(
		context, value, 0, 255, &volume, exception);
	if (err)
		return false;

	err = audio_set_volume(audio, volume);
	if (err)
		javascript_set_exception_text(context, exception,
			"failed to set volume");

	return err == 0;
}

static JSValueRef js_audio_get_volume(JSContextRef context, JSObjectRef object,
		JSStringRef name, JSValueRef *exception)
{
	struct audio *audio = JSObjectGetPrivate(object);
	uint8_t volume;
	int err;

	if (!audio) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	err = audio_get_volume(audio, &volume);
	if (err) {
		javascript_set_exception_text(context, exception,
			"failed to get volume");
		return NULL;
	}

	return javascript_int_to_unit(context, volume, 0, 255);
}

static bool js_audio_set_speakers_enable(JSContextRef context,
		JSObjectRef object, JSStringRef name, JSValueRef value,
		JSValueRef *exception)
{
	struct audio *audio = JSObjectGetPrivate(object);
	int err;

	if (!audio) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return false;
	}

	err = audio_set_speakers_enable(
		audio, JSValueToBoolean(context, value));
	if (err)
		javascript_set_exception_text(context, exception,
			"failed to set speakers enable");

	return err == 0;
}

static JSValueRef js_audio_get_speakers_enable(JSContextRef context,
		JSObjectRef object, JSStringRef name, JSValueRef *exception)
{
	struct audio *audio = JSObjectGetPrivate(object);
	bool enable;
	int err;

	if (!audio) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return false;
	}

	err = audio_get_speakers_enable(audio, &enable);
	if (err) {
		javascript_set_exception_text(context, exception,
			"failed to get speakers enable");
		return NULL;
	}

	return JSValueMakeBoolean(context, enable);
}

static const JSStaticValue audio_properties[] = {
	{ /* The current audio state as a String */
		.name = "state",
		.getProperty = js_audio_get_state,
		.setProperty = js_audio_set_state,
		.attributes = kJSPropertyAttributeDontDelete,
	},
	{ /* The volume as a number in the range 0-1 */
		.name = "volume",
		.getProperty = js_audio_get_volume,
		.setProperty = js_audio_set_volume,
		.attributes = kJSPropertyAttributeDontDelete,
	},
	{ /* The speakers enable as a boolean */
		.name = "speakersEnable",
		.getProperty = js_audio_get_speakers_enable,
		.setProperty = js_audio_set_speakers_enable,
		.attributes = kJSPropertyAttributeDontDelete,
	},
	{}
};

static const JSClassDefinition audio_classdef = {
	.className = "Audio",
	.staticValues = audio_properties,
};

static JSObjectRef javascript_audio_create(
	JSContextRef js, JSClassRef class,
	struct javascript_userdata *user_data)
{
	JSObjectRef self, player;
	struct audio *audio;

	if (!user_data->rcd || !user_data->rcd->rc)
		return NULL;

	audio = remote_control_get_audio(user_data->rcd->rc);
	if (!audio)
		return NULL;

	self = JSObjectMake(js, class, audio);
	if (!self)
		return NULL;

	player = javascript_audio_player_create(
		js, javascript_audio_player.class, user_data);
	if (player)
		javascript_object_set_property(
			js, self, "Player", player,
			kJSPropertyAttributeDontDelete |
			kJSPropertyAttributeReadOnly,
			NULL);

	return self;
}

struct javascript_module javascript_audio = {
	.classdef = &audio_classdef,
	.create = javascript_audio_create,
};
