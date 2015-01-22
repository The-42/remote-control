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

#define VOIP_STATE(v, n) { .value = VOIP_STATE_##v, .name = n }

static const struct javascript_enum voip_state_enum[] = {
	VOIP_STATE(LOGGED_OUT,	"logged-out"),
	VOIP_STATE(LOGGED_IN,	"logged-in"),
	{}
};

#define VOIP_TRANSPORT(v, n) { .value = VOIP_TRANSPORT_##v, .name = n }

static const struct javascript_enum voip_transport_enum[] = {
	VOIP_TRANSPORT(UDP,	"udp"),
	VOIP_TRANSPORT(TCP,	"tcp"),
	VOIP_TRANSPORT(TLS,	"tls"),
	{}
};

static JSValueRef js_voip_get_login_state(
	JSContextRef context, JSObjectRef object,
	JSStringRef name, JSValueRef *exception)
{
	struct voip *voip = JSObjectGetPrivate(object);
	enum voip_state state;
	int err;

	if (!voip) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	err = voip_get_state(voip, &state);
	if (err) {
		javascript_set_exception_text(context, exception,
			"failed to get VoIP login state");
		return NULL;
	}

	return javascript_enum_to_string(
		context, voip_state_enum, state, exception);
}

static JSValueRef js_voip_get_contact_name(
	JSContextRef context, JSObjectRef object,
	JSStringRef name, JSValueRef *exception)
{
	struct voip *voip = JSObjectGetPrivate(object);
	const char *contact = NULL;
	int err;

	if (!voip) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	err = voip_get_contact(voip, &contact, NULL);
	if (err) {
		javascript_set_exception_text(context, exception,
			"failed to get contact");
		return NULL;
	}

	return javascript_make_string(context, contact, exception);
}

static JSValueRef js_voip_get_contact_display(
	JSContextRef context, JSObjectRef object,
	JSStringRef name, JSValueRef *exception)
{
	struct voip *voip = JSObjectGetPrivate(object);
	const char *contact = NULL;
	int err;

	if (!voip) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	err = voip_get_contact(voip, NULL, &contact);
	if (err) {
		javascript_set_exception_text(context, exception,
			"failed to get contact");
		return NULL;
	}

	return javascript_make_string(context, contact, exception);
}

static const JSStaticValue voip_properties[] = {
	{
		.name = "loginState",
		.getProperty = js_voip_get_login_state,
		.attributes = kJSPropertyAttributeDontDelete |
			kJSPropertyAttributeReadOnly,
	},
	{
		.name = "contactName",
		.getProperty = js_voip_get_contact_name,
		.attributes = kJSPropertyAttributeDontDelete |
			kJSPropertyAttributeReadOnly,
	},
	{
		.name = "contactDisplay",
		.getProperty = js_voip_get_contact_display,
		.attributes = kJSPropertyAttributeDontDelete |
			kJSPropertyAttributeReadOnly,
	},
	{}
};

static JSValueRef js_voip_login(
	JSContextRef context, JSObjectRef function, JSObjectRef object,
	size_t argc, const JSValueRef argv[], JSValueRef *exception)
{
	struct voip *voip = JSObjectGetPrivate(object);
	int transport = VOIP_TRANSPORT_UDP;
	char *username = NULL;
	char *password = NULL;
	char *server = NULL;
	int port;
	int err;

	if (!voip) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	if (argc < 4 || argc > 5) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_ARG_COUNT);
		return NULL;
	}

	server = javascript_get_string(context, argv[0], exception);
	if (!server)
		return NULL;

	err = javascript_int_from_number(
		context, argv[1], 0, UINT16_MAX, &port, exception);
	if (err)
		goto on_error;

	username = javascript_get_string(context, argv[2], exception);
	if (!username)
		goto on_error;

	password = javascript_get_string(context, argv[3], exception);
	if (!password)
		goto on_error;

	if (argc > 4) {
		err = javascript_enum_from_string(context, voip_transport_enum,
						argv[4], &transport, exception);
		if (err)
			goto on_error;
	}

	err = voip_login(voip, server, port, username, password, transport);
	if (err)
		javascript_set_exception_text(context, exception,
			"VoIP login failed");

on_error:
	g_free(password);
	g_free(username);
	g_free(server);
	return NULL;
}

static JSValueRef js_voip_logout(
	JSContextRef context, JSObjectRef function, JSObjectRef object,
	size_t argc, const JSValueRef argv[], JSValueRef *exception)
{
	struct voip *voip = JSObjectGetPrivate(object);
	int err;

	if (!voip) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	if (argc > 0) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_ARG_COUNT);
		return NULL;
	}

	err = voip_logout(voip);
	if (err)
		javascript_set_exception_text(context, exception,
			"VoIP logout failed");

	return NULL;
}

static JSValueRef js_voip_call(
	JSContextRef context, JSObjectRef function, JSObjectRef object,
	size_t argc, const JSValueRef argv[], JSValueRef *exception)
{
	struct voip *voip = JSObjectGetPrivate(object);
	char *url;
	int err;

	if (!voip) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	if (argc != 1) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_ARG_COUNT);
		return NULL;
	}

	url = javascript_get_string(context, argv[0], exception);
	if (!url)
		return NULL;

	err = voip_call(voip, url);
	if (err)
		javascript_set_exception_text(context, exception,
			"VoIP logout failed");

	g_free(url);

	return NULL;
}

static JSValueRef js_voip_accept(
	JSContextRef context, JSObjectRef function, JSObjectRef object,
	size_t argc, const JSValueRef argv[], JSValueRef *exception)
{
	struct voip *voip = JSObjectGetPrivate(object);
	char *caller = NULL;
	JSValueRef ret;
	int err;

	if (!voip) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	if (argc > 0) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_ARG_COUNT);
		return NULL;
	}

	err = voip_accept(voip, &caller);
	if (err)
		javascript_set_exception_text(context, exception,
			"VoIP accept failed");

	ret = javascript_make_string(context, caller, exception);
	g_free(caller);

	return ret;
}

static JSValueRef js_voip_terminate(
	JSContextRef context, JSObjectRef function, JSObjectRef object,
	size_t argc, const JSValueRef argv[], JSValueRef *exception)
{
	struct voip *voip = JSObjectGetPrivate(object);
	int err;

	if (!voip) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	if (argc > 0) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_ARG_COUNT);
		return NULL;
	}

	err = voip_terminate(voip);
	if (err)
		javascript_set_exception_text(context, exception,
			"VoIP terminate failed");

	return NULL;
}

static JSValueRef js_voip_dial(
	JSContextRef context, JSObjectRef function, JSObjectRef object,
	size_t argc, const JSValueRef argv[], JSValueRef *exception)
{
	struct voip *voip = JSObjectGetPrivate(object);
	char *digits;
	int err;
	int i;

	if (!voip) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	if (argc != 1) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_ARG_COUNT);
		return NULL;
	}

	digits = javascript_get_string(context, argv[0], exception);
	if (!digits)
		return NULL;

	for (i = 0; digits[i]; i++) {
		if (!((digits[i] >= '0' && digits[i] <= '9') ||
			digits[i] == '#' || digits[i] == '*' ||
			(digits[i] >= 'A' && digits[i] <= 'D') ||
			(digits[i] >= 'a' && digits[i] <= 'd')))
			continue;

		err = voip_dial(voip, digits[i]);
		if (err) {
			javascript_set_exception_text(context, exception,
				"failed to dial DTMF");
			break;
		}
	}

	g_free(digits);
	return NULL;
}

static const JSStaticFunction voip_functions[] = {
	{
		.name = "login",
		.callAsFunction = js_voip_login,
		.attributes = kJSPropertyAttributeDontDelete,
	},
	{
		.name = "logout",
		.callAsFunction = js_voip_logout,
		.attributes = kJSPropertyAttributeDontDelete,
	},
	{
		.name = "call",
		.callAsFunction = js_voip_call,
		.attributes = kJSPropertyAttributeDontDelete,
	},
	{
		.name = "accept",
		.callAsFunction = js_voip_accept,
		.attributes = kJSPropertyAttributeDontDelete,
	},
	{
		.name = "terminate",
		.callAsFunction = js_voip_terminate,
		.attributes = kJSPropertyAttributeDontDelete,
	},
	{
		.name = "dial",
		.callAsFunction = js_voip_dial,
		.attributes = kJSPropertyAttributeDontDelete,
	},
	{}
};

static const JSClassDefinition voip_classdef = {
	.className = "VoIP",
	.staticValues = voip_properties,
	.staticFunctions = voip_functions,
};

static JSObjectRef javascript_voip_create(
	JSContextRef js, JSClassRef class,
	struct javascript_userdata *user_data)
{
	struct voip *voip;

	if (!user_data->rcd || !user_data->rcd->rc)
		return NULL;

	voip = remote_control_get_voip(user_data->rcd->rc);
	if (!voip)
		return NULL;

	return JSObjectMake(js, class, voip);
}

struct javascript_module javascript_voip = {
	.classdef = &voip_classdef,
	.create = javascript_voip_create,
};
