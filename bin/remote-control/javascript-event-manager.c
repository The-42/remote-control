/*
 * Copyright (C) 2016 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <errno.h>

#include "javascript.h"

struct js_event_manager {
	GSource source;
	struct event_manager *manager;
	JSContextRef context;
	JSObjectRef callback;
	JSObjectRef this;
	GList *events;
};

#define SOURCE_ENUM(v, n) { .value = EVENT_SOURCE_##v, .name = n }

static const struct javascript_enum event_manager_source_enum[] = {
	SOURCE_ENUM(SMARTCARD, "smartcard"),
	SOURCE_ENUM(HOOK, "hook"),
	{}
};

static JSValueRef js_event_manager_get_event_state(JSContextRef context,
		struct event *event)
{
	switch (event->source) {
	case EVENT_SOURCE_SMARTCARD:
		switch (event->smartcard.state) {
		case EVENT_SMARTCARD_STATE_REMOVED:
			return JSValueMakeBoolean(context, false);
		case EVENT_SMARTCARD_STATE_INSERTED:
			return JSValueMakeBoolean(context, true);
		default:
			break;
		}
	case EVENT_SOURCE_HOOK:
		switch (event->hook.state) {
		case EVENT_HOOK_STATE_OFF:
			return JSValueMakeBoolean(context, false);
		case EVENT_HOOK_STATE_ON:
			return JSValueMakeBoolean(context, true);
		default:
			break;
		}
	default:
		break;
	}

	return JSValueMakeUndefined(context);
}

int js_event_manager_event_cb(void *data, struct event *event)
{
	struct js_event_manager *priv = (struct js_event_manager *)data;
	struct event *append;

	if (!priv || !event)
		return -EINVAL;

	switch (event->source) {
	case EVENT_SOURCE_SMARTCARD:
	case EVENT_SOURCE_HOOK:
		break;
	default:
		return -ENXIO;
	}

	append = (struct event *)g_memdup(event, sizeof(*event));
	if (!append)
		return -ENOMEM;

	priv->events = g_list_append(priv->events, append);

	return 0;
}

static gboolean js_event_manager_source_prepare(GSource *source, gint *timeout)
{
	if (timeout)
		*timeout = 100;

	return FALSE;
}

static gboolean js_event_manager_source_check(GSource *source)
{
	struct js_event_manager *priv = (struct js_event_manager *)source;
	if (priv && g_list_first(priv->events))
		return TRUE;

	return FALSE;
}

static void js_event_manager_send_event(struct js_event_manager *priv,
		struct event *event)
{
	JSValueRef exception = NULL;
	JSValueRef args[2];

	args[0] = javascript_enum_to_string(priv->context,
			event_manager_source_enum, event->source, &exception);
	args[1] = js_event_manager_get_event_state(priv->context, event);

	(void)JSObjectCallAsFunction(priv->context, priv->callback, priv->this,
			G_N_ELEMENTS(args), args, &exception);
	if (exception)
		g_warning(JS_LOG_CALLBACK_EXCEPTION, __func__);
}

static gboolean js_event_manager_source_dispatch(GSource *source,
		GSourceFunc callback, gpointer user_data)
{
	struct js_event_manager *priv = (struct js_event_manager *)source;
	GList *node = priv ? g_list_first(priv->events) : NULL;
	struct event *event = node ? node->data : NULL;

	while (event) {
		if (priv->context && priv->callback)
			js_event_manager_send_event(priv, event);
		priv->events = g_list_remove(priv->events, event);
		g_free(event);
		node = g_list_first(priv->events);
		event = node ? node->data : NULL;
	}

	if (callback)
		return callback(user_data);

	return TRUE;
}

static void js_event_manager_source_finalize(GSource *source)
{
	struct js_event_manager *priv = (struct js_event_manager *)source;

	g_list_free_full(priv->events, g_free);
}

static GSourceFuncs js_event_manager_source_funcs = {
	.prepare = js_event_manager_source_prepare,
	.check = js_event_manager_source_check,
	.dispatch = js_event_manager_source_dispatch,
	.finalize = js_event_manager_source_finalize,
};

static JSValueRef js_event_manager_get_on_state_changed(JSContextRef context,
		JSObjectRef object, JSStringRef name, JSValueRef *exception)
{
	struct js_event_manager *priv = JSObjectGetPrivate(object);

	if (!priv) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	return priv->callback ? JSValueMakeNull(context) : priv->callback;
}

static bool js_event_manager_set_on_state_changed(JSContextRef context,
		JSObjectRef object, JSStringRef name, JSValueRef value,
		JSValueRef *exception)
{
	struct js_event_manager *priv = JSObjectGetPrivate(object);

	if (!priv) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return false;
	}

	if (priv->callback)
		JSValueUnprotect(context, priv->callback);

	if (JSValueIsNull(context, value)) {
		void *owner = event_manager_get_event_cb_owner(priv->manager,
			js_event_manager_event_cb);

		if ((void *)priv->context == owner) /* Only if we have set the callback */
			event_manager_set_event_cb(priv->manager,
					NULL, NULL, (void *)priv->context);
		priv->callback = NULL;
		return true;
	}

	priv->callback = JSValueToObject(context, value, exception);
	if (!priv->callback) {
		javascript_set_exception_text(context, exception,
			"failed to set on state changed");
		return false;
	}
	JSValueProtect(context, priv->callback);

	if (event_manager_set_event_cb(priv->manager, js_event_manager_event_cb,
			priv, (void *)priv->context)) {
		javascript_set_exception_text(context, exception,
			"failed to set state changed callback");
		return false;
	}

	return true;
}

static const JSStaticValue event_manager_properties[] = {
	{
		.name = "onStateChanged",
		.getProperty = js_event_manager_get_on_state_changed,
		.setProperty = js_event_manager_set_on_state_changed,
		.attributes = kJSPropertyAttributeNone,
	}, {
	}
};

static JSValueRef event_manager_function_state(JSContextRef context,
		JSObjectRef function, JSObjectRef object,
		size_t argc, const JSValueRef argv[], JSValueRef *exception)
{
	struct js_event_manager *priv = JSObjectGetPrivate(object);
	struct event event;

	if (argc != 1) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_ARG_COUNT);
		return NULL;
	}

	if (javascript_enum_from_string(priv->context,
			event_manager_source_enum, argv[0],
			(int *)&event.source, exception)) {
		if (exception && !*exception)
			javascript_set_exception_text(context, exception,
					"failed to get source enum");
		return NULL;
	}

	if (event_manager_get_source_state(priv->manager, &event)) {
		javascript_set_exception_text(context, exception,
				"failed to get source state");
		return NULL;
	}

	return js_event_manager_get_event_state(context, &event);
}

static const JSStaticFunction event_manager_functions[] = {
	{
		.name = "getState",
		.callAsFunction = event_manager_function_state,
		.attributes = kJSPropertyAttributeNone,
	}, {
	}
};

static void event_manager_initialize(JSContextRef context, JSObjectRef object)
{
	struct js_event_manager *priv = JSObjectGetPrivate(object);

	priv->this = object;
}

static void event_manager_finalize(JSObjectRef object)
{
	struct js_event_manager *priv = JSObjectGetPrivate(object);
	void *owner = event_manager_get_event_cb_owner(priv->manager,
		js_event_manager_event_cb);

	if (priv->callback) {
		if ((void *)priv->context == owner) /* Only if we have set the callback */
			event_manager_set_event_cb(priv->manager,
					NULL, NULL, (void *)priv->context);
		JSValueUnprotect(priv->context, priv->callback);
	}
	g_source_destroy(&priv->source);
}

static const JSClassDefinition event_manager_classdef = {
	.className = "EventManager",
	.initialize = event_manager_initialize,
	.finalize = event_manager_finalize,
	.staticValues = event_manager_properties,
	.staticFunctions = event_manager_functions,
};

static JSObjectRef javascript_event_manager_create( JSContextRef context,
		JSClassRef class, struct javascript_userdata *user_data)
{
	struct js_event_manager *priv;
	GSource *source;

	source = g_source_new(&js_event_manager_source_funcs, sizeof(*priv));
	priv = (struct js_event_manager *)source;
	if (!priv)
		return NULL;

	priv->context = context;
	priv->manager = remote_control_get_event_manager(user_data->rcd->rc);

	g_source_attach(source, g_main_loop_get_context(user_data->loop));
	return JSObjectMake(context, class, source);
}

struct javascript_module javascript_event_manager = {
	.classdef = &event_manager_classdef,
	.create = javascript_event_manager_create,
};
