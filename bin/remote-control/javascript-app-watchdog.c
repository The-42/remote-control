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

struct app_watchdog {
	RemoteControlWebkitWindow *window;
	GMainContext *main_context;
	GSource *timeout_source;
	guint interval;

	JSContextRef context;
	JSObjectRef callback;
	JSObjectRef this;
};

static gboolean app_watchdog_timeout(gpointer data)
{
	g_critical("WATCHDOG: It seems the user interface is stalled, restarting");
	raise(SIGTERM);
	return FALSE;
}

static JSValueRef app_watchdog_function_start(
	JSContextRef context, JSObjectRef function, JSObjectRef object,
	size_t argc, const JSValueRef argv[], JSValueRef *exception)
{
	struct app_watchdog *priv = JSObjectGetPrivate(object);

	if (!priv) {
		javascript_set_exception_text(context, exception,
			"object notvalid, context switched?");
		return JSValueMakeBoolean(context, FALSE);
	}

	if (argc != 1) {
		javascript_set_exception_text(context, exception,
			"invalid argument count");
		return JSValueMakeBoolean(context, FALSE);
	}

	if (!JSValueIsNumber(context, argv[0])) {
		javascript_set_exception_text(context, exception,
			"timeout is not a number");
		return JSValueMakeBoolean(context, FALSE);
	}

	if (priv->timeout_source != NULL) {
		javascript_set_exception_text(context, exception,
			"watchdog is already running, call stop() first.");
		return JSValueMakeBoolean(context, FALSE);
	}

	priv->interval = JSValueToNumber(context, argv[0], exception);

	if (priv->timeout_source != NULL)
		g_source_destroy(priv->timeout_source);

	priv->timeout_source = g_timeout_source_new_seconds(priv->interval);
	if (!priv->timeout_source) {
		javascript_set_exception_text(context, exception,
			"watchdog timeout source could not be created.");
		return JSValueMakeBoolean(context, FALSE);
	}

	g_source_set_callback(priv->timeout_source, app_watchdog_timeout,
		priv, NULL);
	g_source_attach(priv->timeout_source, priv->main_context);

	return 0;
}

static JSValueRef app_watchdog_function_stop(JSContextRef context,
	JSObjectRef function, JSObjectRef object, size_t argc,
	const JSValueRef argv[], JSValueRef *exception)
{
	struct app_watchdog *priv = JSObjectGetPrivate(object);

	if (!priv) {
		javascript_set_exception_text(context, exception,
			"object notvalid, context switched?");
		return JSValueMakeBoolean(context, FALSE);
	}

	if (priv->timeout_source != NULL)
		g_source_destroy(priv->timeout_source);

	priv->timeout_source = NULL;

	return 0;
}

static JSValueRef app_watchdog_function_trigger(
	JSContextRef context, JSObjectRef function, JSObjectRef object,
	size_t argc, const JSValueRef argv[], JSValueRef *exception)
{
	struct app_watchdog *priv = JSObjectGetPrivate(object);

	if (!priv) {
		javascript_set_exception_text(context, exception,
			"object notvalid, context switched?");
		return JSValueMakeBoolean(context, FALSE);
	}

	if (priv->timeout_source == NULL) {
		javascript_set_exception_text(context, exception,
			"watchdog is not running, call start() first.");
		return JSValueMakeBoolean(context, FALSE);
	}

	g_source_destroy(priv->timeout_source);
	priv->timeout_source = g_timeout_source_new_seconds(priv->interval);
	if (!priv->timeout_source) {
		javascript_set_exception_text(context, exception,
			"watchdog timeout source could not be created.");
		return JSValueMakeBoolean(context, FALSE);
	}

	g_source_set_callback(priv->timeout_source, app_watchdog_timeout,
		priv, NULL);
	g_source_attach(priv->timeout_source, priv->main_context);

	return 0;
}

static struct app_watchdog *app_watchdog_new(JSContextRef context,
	struct javascript_userdata *data)
{
	struct app_watchdog *watchdog;

	watchdog = g_new0(struct app_watchdog, 1);
	if (!watchdog) {
		g_warning("%s: failed to allocate memory", __func__);
		return NULL;
	}

	watchdog->main_context = g_main_loop_get_context(data->loop);
	watchdog->window = data->window;

	return watchdog;
}

static void app_watchdog_finalize(JSObjectRef object)
{
	struct app_watchdog *watchdog = JSObjectGetPrivate(object);
	if(watchdog->timeout_source != NULL)
		g_source_destroy(watchdog->timeout_source);
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
	.finalize = app_watchdog_finalize,
	.staticFunctions = app_watchdog_functions,
};

static JSClassRef app_watchdog_class = NULL;

int javascript_register_app_watchdog_class(void)
{
	app_watchdog_class = JSClassCreate(&app_watchdog_classdef);
	if (!app_watchdog_class) {
		g_warning("%s: failed to create Watchdog class",
			__func__);
		return -ENOMEM;
	}

	return 0;
}

int javascript_register_app_watchdog(JSContextRef js,
	JSObjectRef parent, const char *name, void *user_data)
{
	JSValueRef exception = NULL;
	JSObjectRef object;
	JSStringRef string;
	struct app_watchdog *watchdog;

	watchdog = app_watchdog_new(js, user_data);
	if (!watchdog)
		return -ENOMEM;

	object = JSObjectMake(js, app_watchdog_class, watchdog);

	string = JSStringCreateWithUTF8CString(name);
	JSObjectSetProperty(js, parent, string, object, 0, &exception);
	JSStringRelease(string);

	return 0;
}
