/*
 * Copyright (C) 2012 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <JavaScriptCore/JavaScript.h>
#include <glib.h>
#include <errno.h>
#include <math.h>

#include "javascript.h"

extern struct javascript_module javascript_cursor;
extern struct javascript_module javascript_input;
extern struct javascript_module javascript_cursor;
extern struct javascript_module javascript_lcd;
extern struct javascript_module javascript_app_watchdog;
extern struct javascript_module javascript_monitor;
extern struct javascript_module javascript_taskmanager;
extern struct javascript_module javascript_audio;
extern struct javascript_module javascript_backlight;
extern struct javascript_module javascript_media_player;
extern struct javascript_module javascript_modem;
extern struct javascript_module javascript_voip;

static struct javascript_module *ad_modules[] = {
	&javascript_cursor,
	&javascript_input,
#ifdef ENABLE_JAVASCRIPT_IR
	&javascript_cursor,
#endif
#ifdef ENABLE_JAVASCRIPT_LCD
	&javascript_lcd,
#endif
#ifdef ENABLE_JAVASCRIPT_APP_WATCHDOG
	&javascript_app_watchdog,
#endif
	&javascript_monitor,
	&javascript_taskmanager,
	&javascript_audio,
	&javascript_backlight,
	&javascript_media_player,
	&javascript_modem,
	&javascript_voip,
	NULL
};

JSValueRef javascript_make_string(
	JSContextRef context, const char *cstr, JSValueRef *exception)
{
	JSValueRef value;
	JSStringRef str;

	if (cstr == NULL)
		return NULL;

	str = JSStringCreateWithUTF8CString(cstr);
	if (!str) {
		javascript_set_exception_text(context, exception,
			"failed to create string");
		return NULL;
	}

	value = JSValueMakeString(context, str);
	JSStringRelease(str);

	return value;
}

void javascript_set_exception_text(JSContextRef context,JSValueRef *exception,
                               const char *failure)
{
	if (exception) {
		*exception = javascript_make_string(
			context, failure, exception);
	}
}

char* javascript_get_string(JSContextRef context, const JSValueRef val,
			JSValueRef *exception)
{
	JSStringRef str;
	size_t size;
	char *buffer;

	str = JSValueToStringCopy(context, val, exception);
	if (!str)
		return NULL;

	size = JSStringGetMaximumUTF8CStringSize(str);
	buffer = g_malloc(size);

	if (buffer)
		JSStringGetUTF8CString(str, buffer, size);
	else
		javascript_set_exception_text(context, exception,
			"failed to allocated string buffer");

	JSStringRelease(str);
	return buffer;
}

int javascript_int_from_number(JSContextRef context, const JSValueRef val,
		int min, int max, int *ret, JSValueRef *exception)
{
	double dval;

	dval = JSValueToNumber(context, val, exception);
	if (dval == NAN)
		return -EINVAL;

	if (!ret)
		return 0;

	if (dval < min)
		*ret = min;
	else if (dval > max)
		*ret = max;
	else
		*ret = dval;

	return 0;
}

int javascript_int_from_unit(JSContextRef context, const JSValueRef val,
		int min, int max, int *ret, JSValueRef *exception)
{
	double dval;

	dval = JSValueToNumber(context, val, exception);
	if (dval == NAN)
		return -EINVAL;

	if (!ret)
		return 0;

	dval = dval * (max - min) + min;
	if (dval < min)
		*ret = min;
	else if (dval > max)
		*ret = max;
	else
		*ret = dval;

	return 0;
}

JSValueRef javascript_int_to_unit(JSContextRef context, int val,
		int min, int max)
{
	double dval = (double)(val - min) / (max - min);
	return JSValueMakeNumber(context, dval);
}

int javascript_enum_from_string(
	JSContextRef context, const struct javascript_enum *desc,
	JSValueRef value, int *ret, JSValueRef *exception)
{
	JSStringRef str;
	int i;

	str = JSValueToStringCopy(context, value, exception);
	if (!str)
		return -ENOMEM;

	for (i = 0; desc[i].name; i++) {
		if (JSStringIsEqualToUTF8CString(str, desc[i].name))
			break;
	}

	JSStringRelease(str);

	if (!desc[i].name) {
		javascript_set_exception_text(context, exception,
					"unknown enum value");
		return -EINVAL;
	}

	if (ret)
		*ret = desc[i].value;

	return 0;
}

JSValueRef javascript_enum_to_string(
	JSContextRef context, const struct javascript_enum *desc,
	int val, JSValueRef *exception)
{
	JSValueRef value;
	JSStringRef str;
	int i;

	for (i = 0; desc[i].name; i++) {
		if (desc[i].value == val)
			break;
	}

	if (!desc[i].name) {
		javascript_set_exception_text(context, exception,
					"got unknown enum value");
		return NULL;
	}

	str = JSStringCreateWithUTF8CString(desc[i].name);
	if (!str) {
		javascript_set_exception_text(context, exception,
					"failed to create string");
		return NULL;
	}

	value = JSValueMakeString(context, str);
	JSStringRelease(str);
	return value;
}

static int javascript_register_module(JSGlobalContextRef js,
				JSObjectRef parent,
				struct javascript_module *module,
				struct javascript_userdata *data)
{
	JSObjectRef object;
	JSStringRef string;

	if (!module->create)
		return 0;

	string = JSStringCreateWithUTF8CString(module->classdef->className);
	if (!string)
		return -ENOMEM;

	object = module->create(js, module->class, data);
	if (object)
		JSObjectSetProperty(js, parent, string, object, 0, NULL);

	JSStringRelease(string);
	return object ? 0 : -ENODEV;
}

static int javascript_register_avionic_design(JSGlobalContextRef js,
                                              JSObjectRef parent,
                                              const char *name,
					      struct javascript_userdata *data)
{
	JSValueRef exception = NULL;
	JSObjectRef object;
	JSStringRef string;
	int i, err;

	object = JSObjectMake(js, NULL, NULL);

	for (i = 0; ad_modules[i]; i++) {
		err = javascript_register_module(
			js, object, ad_modules[i], data);
		if (err) {
			g_warning("%s: failed to register %s object: %s",
				__func__, ad_modules[i]->classdef->className,
				g_strerror(-err));
			if (err != -ENODEV)
				return err;
		}
	}

	string = JSStringCreateWithUTF8CString(name);
	if (string) {
		JSObjectSetProperty(js, parent, string, object, 0, &exception);
		JSStringRelease(string);
	}

	return 0;
}

static int javascript_register_classes(void)
{
	int i;

	for (i = 0; ad_modules[i]; i++) {
		ad_modules[i]->class = JSClassCreate(ad_modules[i]->classdef);
		if (!ad_modules[i]->class) {
			g_warning("%s: failed to register %s class",
				__func__, ad_modules[i]->classdef->className);
			return -ENOMEM;
		}
	}

	return 0;
}

int javascript_register(JSGlobalContextRef context,
                        struct javascript_userdata *user_data)
{
	JSObjectRef object;
	int err;

	err = javascript_register_classes();
	if (err < 0) {
		g_debug("failed to register JavaScript classes: %s",
				g_strerror(-err));
		return err;
	}

	object = JSContextGetGlobalObject(context);

	err = javascript_register_avionic_design(context, object,
	                                         "AvionicDesign",
	                                         user_data);
	if (err < 0) {
		g_debug("failed to register AvionicDesign object: %s",
				g_strerror(-err));
		return err;
	}

	return 0;
}
