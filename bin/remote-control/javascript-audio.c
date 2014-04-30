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

static bool js_audio_set_state(JSContextRef context, JSObjectRef object,
		JSStringRef name, JSValueRef value, JSValueRef *exception)
{
	struct remote_control *rc = JSObjectGetPrivate(object);
	struct audio *audio = remote_control_get_audio(rc);
	enum audio_state state;
	int err;

	if (!audio) {
		javascript_set_exception_text(context, exception,
			"object not valid, context switched?");
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
	struct remote_control *rc = JSObjectGetPrivate(object);
	struct audio *audio = remote_control_get_audio(rc);
	enum audio_state state;
	int err;

	if (!audio) {
		javascript_set_exception_text(context, exception,
			"object not valid, context switched?");
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
	struct remote_control *rc = JSObjectGetPrivate(object);
	struct audio *audio = remote_control_get_audio(rc);
	int volume;
	int err;

	if (!audio) {
		javascript_set_exception_text(context, exception,
			"object not valid, context switched?");
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
	struct remote_control *rc = JSObjectGetPrivate(object);
	struct audio *audio = remote_control_get_audio(rc);
	uint8_t volume;
	int err;

	if (!audio) {
		javascript_set_exception_text(context, exception,
			"object not valid, context switched?");
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
	struct remote_control *rc = JSObjectGetPrivate(object);
	struct audio *audio = remote_control_get_audio(rc);
	int err;

	if (!audio) {
		javascript_set_exception_text(context, exception,
			"object not valid, context switched?");
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
	struct remote_control *rc = JSObjectGetPrivate(object);
	struct audio *audio = remote_control_get_audio(rc);
	bool enable;
	int err;

	if (!audio) {
		javascript_set_exception_text(context, exception,
			"object not valid, context switched?");
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
	if (!user_data->rcd || !user_data->rcd->rc)
		return NULL;

	return JSObjectMake(js, class, user_data->rcd->rc);
}

struct javascript_module javascript_audio = {
	.classdef = &audio_classdef,
	.create = javascript_audio_create,
};
