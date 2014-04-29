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

#include "javascript.h"

static bool js_backlight_set_brightness(JSContextRef context, JSObjectRef object,
		JSStringRef name, JSValueRef value, JSValueRef *exception)
{
	struct backlight *backlight = JSObjectGetPrivate(object);
	int brightness;
	int err;

	if (!backlight) {
		javascript_set_exception_text(context, exception,
			"object not valid, context switched?");
		return false;
	}

	err = javascript_int_from_unit(
		context, value, BACKLIGHT_MIN, BACKLIGHT_MAX,
		&brightness, exception);
	if (err)
		return false;

	err = backlight_set(backlight, brightness);
	if (err)
		javascript_set_exception_text(context, exception,
			"failed to set brightness");

	return err == 0;
}

static JSValueRef js_backlight_get_brightness(JSContextRef context, JSObjectRef object,
		JSStringRef name, JSValueRef *exception)
{
	struct backlight *backlight = JSObjectGetPrivate(object);
	int brightness;

	if (!backlight) {
		javascript_set_exception_text(context, exception,
			"object not valid, context switched?");
		return NULL;
	}

	brightness = backlight_get(backlight);
	if (brightness < 0) {
		javascript_set_exception_text(context, exception,
			"failed to get brightness");
		return NULL;
	}

	return javascript_int_to_unit(context, brightness,
				BACKLIGHT_MIN, BACKLIGHT_MAX);
}

static bool js_backlight_set_enable(JSContextRef context,
		JSObjectRef object, JSStringRef name, JSValueRef value,
		JSValueRef *exception)
{
	struct backlight *backlight = JSObjectGetPrivate(object);
	int err;

	if (!backlight) {
		javascript_set_exception_text(context, exception,
			"object not valid, context switched?");
		return false;
	}

	err = backlight_enable(backlight, JSValueToBoolean(context, value));
	if (err)
		javascript_set_exception_text(context, exception,
			"failed to set backlight enable");

	return err == 0;
}


static JSValueRef js_backlight_get_enable(JSContextRef context,
		JSObjectRef object, JSStringRef name, JSValueRef *exception)
{
	/* TODO: Replace this with a real implementation when
	 * backlight_is_enabled() has been implemented. */
	javascript_set_exception_text(context, exception,
			"backlight enable can not be queried ATM");
	return false;
}

static const JSStaticValue backlight_properties[] = {
	{ /* The backlight brightness as a number in the range 0-1 */
		.name = "brightness",
		.getProperty = js_backlight_get_brightness,
		.setProperty = js_backlight_set_brightness,
		.attributes = kJSPropertyAttributeDontDelete,
	},
	{ /* The backlight state as a boolean */
		.name = "enable",
		.getProperty = js_backlight_get_enable,
		.setProperty = js_backlight_set_enable,
		.attributes = kJSPropertyAttributeDontDelete,
	},
	{}
};

static const JSClassDefinition backlight_classdef = {
	.className = "Backlight",
	.staticValues = backlight_properties,
};

static JSObjectRef javascript_backlight_create(
	JSContextRef js, JSClassRef class,
	struct javascript_userdata *user_data)
{
	struct backlight *backlight;

	backlight = remote_control_get_backlight(user_data->rcd->rc);
	if (!backlight)
		return NULL;

	return JSObjectMake(js, class, backlight);
}

struct javascript_module javascript_backlight = {
	.classdef = &backlight_classdef,
	.create = javascript_backlight_create,
};
