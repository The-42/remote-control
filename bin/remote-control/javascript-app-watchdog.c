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

#include <errno.h>
#include <signal.h>

#include "javascript.h"

static JSValueRef app_watchdog_function_start(
	JSContextRef context, JSObjectRef function, JSObjectRef object,
	size_t argc, const JSValueRef argv[], JSValueRef *exception)
{
	struct app_watchdog *watchdog = JSObjectGetPrivate(object);
	int ret;

	if (!watchdog) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	if (argc != 1) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_ARG_COUNT);
		return NULL;
	}

	if (!JSValueIsNumber(context, argv[0])) {
		javascript_set_exception_text(context, exception,
			"timeout is not a number");
		return NULL;
	}

	ret = app_watchdog_start(watchdog, JSValueToNumber(context, argv[0], NULL));
	if (ret)
		javascript_set_exception_text(context, exception,
			"Failed to start watchdog");

	return NULL;
}

static JSValueRef app_watchdog_function_stop(JSContextRef context,
	JSObjectRef function, JSObjectRef object, size_t argc,
	const JSValueRef argv[], JSValueRef *exception)
{
	struct app_watchdog *watchdog = JSObjectGetPrivate(object);
	int ret;

	if (!watchdog) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	ret = app_watchdog_stop(watchdog);
	if (ret)
		javascript_set_exception_text(context, exception,
			"Failed to stop watchdog");

	return NULL;
}

static JSValueRef app_watchdog_function_trigger(
	JSContextRef context, JSObjectRef function, JSObjectRef object,
	size_t argc, const JSValueRef argv[], JSValueRef *exception)
{
	struct app_watchdog *watchdog = JSObjectGetPrivate(object);
	int ret;

	if (!watchdog) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	ret = app_watchdog_trigger(watchdog);
	if (ret)
		javascript_set_exception_text(context, exception,
			"Failed to trigger watchdog");

	return NULL;
}

static const JSStaticFunction app_watchdog_functions[] = {
	{
		.name = "start",
		.callAsFunction = app_watchdog_function_start,
		.attributes = kJSPropertyAttributeNone,
	},
	{
		.name = "stop",
		.callAsFunction = app_watchdog_function_stop,
		.attributes = kJSPropertyAttributeNone,
	},
	{
		.name = "trigger",
		.callAsFunction = app_watchdog_function_trigger,
		.attributes = kJSPropertyAttributeNone,
	}, {
	}
};

static const JSClassDefinition app_watchdog_classdef = {
	.className = "Watchdog",
	.staticFunctions = app_watchdog_functions,
};

static JSObjectRef javascript_app_watchdog_create(
	JSContextRef js, JSClassRef class,
	struct javascript_userdata *user_data)
{
	struct app_watchdog *watchdog;

	if (!user_data->rcd || !user_data->rcd->rc)
		return NULL;

	watchdog = remote_control_get_watchdog(user_data->rcd->rc);
	if (!watchdog)
		return NULL;

	return JSObjectMake(js, class, watchdog);
}

struct javascript_module javascript_app_watchdog = {
	.classdef = &app_watchdog_classdef,
	.create = javascript_app_watchdog_create,
};
