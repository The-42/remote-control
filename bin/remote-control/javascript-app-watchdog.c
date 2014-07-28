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

#define JS_APP_WATCHDOG "js-watchdog"
#define JS_APP_WATCHDOG_DEFAULT_TIMEOUT "timeout"
static guint app_watchdog_default_timeout = 0;

static gboolean app_watchdog_timeout(gpointer data)
{
	g_critical("WATCHDOG: It seems the user interface is stalled, restarting");
	raise(SIGTERM);
	return FALSE;
}

static int app_watchdog_start(struct app_watchdog *priv)
{
	if (priv->timeout_source != NULL)
		g_source_destroy(priv->timeout_source);

	priv->timeout_source = g_timeout_source_new_seconds(priv->interval);
	if (!priv->timeout_source)
		return -ENOMEM;

	g_source_set_callback(priv->timeout_source, app_watchdog_timeout,
		priv, NULL);
	g_source_attach(priv->timeout_source, priv->main_context);

	return 0;
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

	if (app_watchdog_start(priv) != 0) {
		javascript_set_exception_text(context, exception,
			"watchdog timeout source could not be created.");
		return JSValueMakeBoolean(context, FALSE);
	}

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

	g_free(watchdog);
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

static int javascript_app_watchdog_init(GKeyFile *config)
{
	app_watchdog_default_timeout = (guint)javascript_config_get_integer(config,
			JS_APP_WATCHDOG, "", JS_APP_WATCHDOG_DEFAULT_TIMEOUT);

	return 0;
}

static JSObjectRef javascript_app_watchdog_create(
	JSContextRef js, JSClassRef class,
	struct javascript_userdata *user_data)
{
	struct app_watchdog *watchdog;

	watchdog = app_watchdog_new(js, user_data);
	if (!watchdog)
		return NULL;

	watchdog->interval = app_watchdog_default_timeout;
	if (watchdog->interval > 0) {
		g_debug("%s: Autostart watchdog with interval %d", __func__,
				watchdog->interval);
		if (app_watchdog_start(watchdog) != 0)
			g_warning("%s: Could not autostart watchdog", __func__);
	}

	return JSObjectMake(js, class, watchdog);
}

struct javascript_module javascript_app_watchdog = {
	.classdef = &app_watchdog_classdef,
	.init = javascript_app_watchdog_init,
	.create = javascript_app_watchdog_create,
};
