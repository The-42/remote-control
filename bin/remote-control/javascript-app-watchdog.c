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

static guint g_id;
static GArray *g_watchdogs;
G_LOCK_DEFINE (g_watchdogs_lock);

struct app_watchdog {
	RemoteControlWebkitWindow *window;
	GMainContext *main_context;
	GSource *timeout_source;
	guint interval;
	guint id;

	JSContextRef context;
	JSObjectRef callback;
	JSObjectRef this;
};

static guint app_watchdogs_register(guint id)
{
	guint i, ret;

	G_LOCK(g_watchdogs_lock);

	if (!g_watchdogs)
		g_watchdogs = g_array_new (FALSE, FALSE, sizeof (gint));
	if (!g_watchdogs)
		goto cleanup;

	for (i = 0; i < g_watchdogs->len; i++)
		if (g_array_index(g_watchdogs, guint, i) == id)
			goto cleanup; /* already registered */

	g_array_append_val(g_watchdogs, id);
cleanup:
	ret = g_watchdogs ? g_watchdogs->len : 0;
	G_UNLOCK(g_watchdogs_lock);

	return ret;
}

static guint app_watchdogs_unregister(guint id)
{
	guint i, ret;

	G_LOCK(g_watchdogs_lock);

	if (!g_watchdogs)
		goto cleanup;

	for (i = 0; i < g_watchdogs->len; i++) {
		if (g_array_index(g_watchdogs, guint, i) == id) {
			g_array_remove_index(g_watchdogs, i);
			goto cleanup;
		}
	}
cleanup:
	ret = g_watchdogs ? g_watchdogs->len : 0;
	if (!ret && g_watchdogs) {
		g_array_free(g_watchdogs, TRUE);
		g_watchdogs = NULL;
	}

	G_UNLOCK(g_watchdogs_lock);

	return ret;
}

static gboolean app_watchdog_timeout(gpointer data)
{
	guint id = (guint)data;
	guint count = app_watchdogs_unregister(id);

	if (count)
		g_warning("WATCHDOG #%u: %u watchdogs are still running", id, count);

	g_critical("WATCHDOG #%u: It seems the user interface is stalled, restarting",
			id);
	raise(SIGTERM);
	return FALSE;
}

static JSValueRef app_watchdog_function_start(
	JSContextRef context, JSObjectRef function, JSObjectRef object,
	size_t argc, const JSValueRef argv[], JSValueRef *exception)
{
	struct app_watchdog *priv = JSObjectGetPrivate(object);
	guint count;

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
		(gpointer)priv->id, NULL);
	g_source_attach(priv->timeout_source, priv->main_context);

	count = app_watchdogs_register(priv->id);
	g_debug("Watchdog #%u started (%u running)", priv->id, count);

	return 0;
}

static JSValueRef app_watchdog_function_stop(JSContextRef context,
	JSObjectRef function, JSObjectRef object, size_t argc,
	const JSValueRef argv[], JSValueRef *exception)
{
	struct app_watchdog *priv = JSObjectGetPrivate(object);
	guint count;

	if (!priv) {
		javascript_set_exception_text(context, exception,
			"object notvalid, context switched?");
		return JSValueMakeBoolean(context, FALSE);
	}

	if (priv->timeout_source != NULL)
		g_source_destroy(priv->timeout_source);

	priv->timeout_source = NULL;

	count = app_watchdogs_unregister(priv->id);
	g_debug("Watchdog #%u stopped (%u remaining)", priv->id, count);

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

	g_debug("Watchdog #%u triggered", priv->id);

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
	watchdog->id = ++g_id;

	g_debug("Watchdog #%u created", watchdog->id);

	return watchdog;
}

static void app_watchdog_finalize(JSObjectRef object)
{
	struct app_watchdog *watchdog = JSObjectGetPrivate(object);
	guint count;

	if(watchdog->timeout_source != NULL)
		g_source_destroy(watchdog->timeout_source);

	count = app_watchdogs_unregister(watchdog->id);
	g_debug("Watchdog #%u destroyed (%u running)", watchdog->id, count);

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


static JSObjectRef javascript_app_watchdog_create(
	JSContextRef js, JSClassRef class,
	struct javascript_userdata *user_data)
{
	struct app_watchdog *watchdog;

	watchdog = app_watchdog_new(js, user_data);
	if (!watchdog)
		return NULL;

	return JSObjectMake(js, class, watchdog);
}

struct javascript_module javascript_app_watchdog = {
	.classdef = &app_watchdog_classdef,
	.create = javascript_app_watchdog_create,
};
