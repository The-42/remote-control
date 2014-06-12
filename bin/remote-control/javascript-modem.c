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

#include "remote-control-data.h"
#include "remote-control.h"
#include "javascript.h"

#define MODEM_STATE(v, n) { .value = MODEM_STATE_##v, .name = n }

static const struct javascript_enum modem_state_enum[] = {
	MODEM_STATE(IDLE,	"idle"),
	MODEM_STATE(RINGING,	"ringing"),
	MODEM_STATE(INCOMING,	"incoming"),
	MODEM_STATE(OUTGOING,	"outgoing"),
	MODEM_STATE(ACTIVE,	"active"),
	{}
};

static JSValueRef js_modem_get_state(
	JSContextRef context, JSObjectRef object,
	JSStringRef name, JSValueRef *exception)
{
	struct modem_manager *modem = JSObjectGetPrivate(object);
	enum modem_state state;
	int err;

	if (!modem) {
		javascript_set_exception_text(context, exception,
			"object not valid, context switched?");
		return NULL;
	}

	err = modem_manager_get_state(modem, &state);
	if (err) {
		javascript_set_exception_text(context, exception,
			"failed to get modem state");
		return NULL;
	}

	return javascript_enum_to_string(
		context, modem_state_enum, state, exception);
}

static const JSStaticValue modem_properties[] = {
	{
		.name = "state",
		.getProperty = js_modem_get_state,
		.attributes = kJSPropertyAttributeDontDelete |
			kJSPropertyAttributeReadOnly,
	},
	{}
};

static JSValueRef js_modem_do(
	JSContextRef context, JSObjectRef function, JSObjectRef object,
	size_t argc, const JSValueRef argv[], JSValueRef *exception,
	int (*action)(struct modem_manager *), const char *error)
{
	struct modem_manager *modem = JSObjectGetPrivate(object);
	int err;

	if (!modem) {
		javascript_set_exception_text(context, exception,
			"object not valid, context switched?");
		return NULL;
	}

	if (argc != 0) {
		javascript_set_exception_text(context, exception,
			"invalid arguments count");
		return NULL;
	}

	err = action(modem);
	if (err)
		javascript_set_exception_text(
			context, exception, "%s", error);

	return NULL;
}

static JSValueRef js_modem_initialize(
	JSContextRef context, JSObjectRef function, JSObjectRef object,
	size_t argc, const JSValueRef argv[], JSValueRef *exception)
{
	return js_modem_do(context, function, object, argc, argv, exception,
			modem_manager_initialize,
			"failed to initialize modem");
}

static JSValueRef js_modem_shutdown(
	JSContextRef context, JSObjectRef function, JSObjectRef object,
	size_t argc, const JSValueRef argv[], JSValueRef *exception)
{
	return js_modem_do(context, function, object, argc, argv, exception,
			modem_manager_shutdown,
			"failed to shutdown modem");
}

static JSValueRef js_modem_call(
	JSContextRef context, JSObjectRef function, JSObjectRef object,
	size_t argc, const JSValueRef argv[], JSValueRef *exception)
{
	struct modem_manager *modem = JSObjectGetPrivate(object);
	char *number;
	int err;

	if (!modem) {
		javascript_set_exception_text(context, exception,
			"object not valid, context switched?");
		return NULL;
	}

	if (argc != 1) {
		javascript_set_exception_text(context, exception,
			"invalid arguments count");
		return NULL;
	}

	number = javascript_get_string(context, argv[0], exception);
	if (!number)
		return NULL;

	err = modem_manager_call(modem, number);
	if (err)
		javascript_set_exception_text(context, exception,
			"failed to start call");

	g_free(number);
	return NULL;
}

static JSValueRef js_modem_accept(
	JSContextRef context, JSObjectRef function, JSObjectRef object,
	size_t argc, const JSValueRef argv[], JSValueRef *exception)
{
	return js_modem_do(context, function, object, argc, argv, exception,
			modem_manager_accept, "failed to accept call");
}

static JSValueRef js_modem_terminate(
	JSContextRef context, JSObjectRef function, JSObjectRef object,
	size_t argc, const JSValueRef argv[], JSValueRef *exception)
{
	return js_modem_do(context, function, object, argc, argv, exception,
			modem_manager_terminate, "failed to terminate call");
}

static const JSStaticFunction modem_functions[] = {
	{
		.name = "initialize",
		.callAsFunction = js_modem_initialize,
		.attributes = kJSPropertyAttributeDontDelete,
	},
	{
		.name = "shutdown",
		.callAsFunction = js_modem_shutdown,
		.attributes = kJSPropertyAttributeDontDelete,
	},
	{
		.name = "call",
		.callAsFunction = js_modem_call,
		.attributes = kJSPropertyAttributeDontDelete,
	},
	{
		.name = "accept",
		.callAsFunction = js_modem_accept,
		.attributes = kJSPropertyAttributeDontDelete,
	},
	{
		.name = "terminate",
		.callAsFunction = js_modem_terminate,
		.attributes = kJSPropertyAttributeDontDelete,
	},
	{}
};

static const JSClassDefinition modem_classdef = {
	.className = "Modem",
	.staticValues = modem_properties,
	.staticFunctions = modem_functions,
};

static JSObjectRef javascript_modem_create(
	JSContextRef js, JSClassRef class,
	struct javascript_userdata *user_data)
{
	struct modem_manager *modem;

	if (!user_data->rcd || !user_data->rcd->rc)
		return NULL;

	modem = remote_control_get_modem_manager(user_data->rcd->rc);
	if (!modem)
		return NULL;

	return JSObjectMake(js, class, modem);
}

struct javascript_module javascript_modem = {
	.classdef = &modem_classdef,
	.create = javascript_modem_create,
};
