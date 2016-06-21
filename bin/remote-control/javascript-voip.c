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

#include "remote-control-data.h"
#include "remote-control.h"
#include "javascript.h"

struct js_voip {
	GSource source;
	struct voip *voip;
	JSContextRef context;
	JSObjectRef state_change_cb;
	JSObjectRef this;
	GList *events;
};

#define VOIP_STATE(v, n) { .value = VOIP_STATE_##v, .name = n }

static const struct javascript_enum voip_state_enum[] = {
	VOIP_STATE(IDLE,			"idle"),
	VOIP_STATE(INCOMING,			"incoming"),
	VOIP_STATE(CONNECTED,			"connected"),
	VOIP_STATE(DISCONNECTED,		"disconnected"),
	VOIP_STATE(INCOMING_EARLYMEDIA,		"incoming-earlymedia"),
	VOIP_STATE(ERROR_USER_BUSY,		"error-user-busy"),
	VOIP_STATE(OUTGOING,			"outgoing"),
	VOIP_STATE(OUTGOING_FAILED,		"outgoing-failed"),
	VOIP_STATE(OUTGOING_EARLYMEDIA,		"outgoing-earlymedia"),
	{}
};

#define VOIP_LOGIN_STATE(v, n) { .value = VOIP_LOGIN_STATE_##v, .name = n }

static const struct javascript_enum voip_login_state_enum[] = {
	VOIP_LOGIN_STATE(LOGGED_OUT,	"logged-out"),
	VOIP_LOGIN_STATE(LOGGED_IN,	"logged-in"),
	{}
};

#define VOIP_TRANSPORT(v, n) { .value = VOIP_TRANSPORT_##v, .name = n }

static const struct javascript_enum voip_transport_enum[] = {
	VOIP_TRANSPORT(UDP,	"udp"),
	VOIP_TRANSPORT(TCP,	"tcp"),
	VOIP_TRANSPORT(TLS,	"tls"),
	{}
};

/* callback called by backend */
void js_voip_state_changed_cb(enum voip_state state, void *data)
{
	struct js_voip *jsvoip = (struct js_voip *)data;
	int *evt;

	if (!jsvoip)
		return;

	evt = g_new0(int, 1);
	if (!evt)
		return;

	*evt = state;
	jsvoip->events = g_list_append(jsvoip->events, evt);
}

/* callback to JS emitted by GSource stuff */
static int js_voip_send_state_change_event(struct js_voip *jsvoip,
		enum voip_state *state)
{
	JSValueRef exception = NULL;
	JSValueRef args[1];

	args[0] = javascript_enum_to_string(jsvoip->context, voip_state_enum,
		*state, &exception);
	if (exception) {
		g_warning("%s: failed to create VoIP state string", __func__);
		return -EFAULT;
	}

	(void)JSObjectCallAsFunction(jsvoip->context, jsvoip->state_change_cb,
			jsvoip->this, G_N_ELEMENTS(args), args, &exception);

	if (exception)
		g_warning(JS_LOG_CALLBACK_EXCEPTION, __func__);

	return 0;
}

static gboolean js_voip_source_prepare(GSource *source, gint *timeout)
{
	if (timeout)
		*timeout = 100;

	return FALSE;
}

static gboolean js_voip_source_check(GSource *source)
{
	struct js_voip *jsvoip = (struct js_voip *)source;

	if (jsvoip && g_list_first(jsvoip->events))
		return TRUE;

	return FALSE;
}

static gboolean js_voip_source_dispatch(GSource *source, GSourceFunc callback,
		gpointer user_data)
{
	struct js_voip *jsvoip = (struct js_voip *)source;
	GList *node = jsvoip ? g_list_first(jsvoip->events) : NULL;
	enum voip_state *state = node ? node->data : NULL;

	while (state) {
		if (jsvoip->context && jsvoip->state_change_cb) {
			int err = js_voip_send_state_change_event(jsvoip, state);
			if (err < 0 && err != -EFAULT)
				g_warning("%s: %s", __func__, g_strerror(-err));
		}
		jsvoip->events = g_list_remove(jsvoip->events, state);
		g_free(state);
		node = g_list_first(jsvoip->events);
		state = node ? node->data : NULL;
	}

	if (callback)
		return callback(user_data);

	return TRUE;
}

static void js_voip_source_finalize(GSource *source)
{
	struct js_voip *jsvoip = (struct js_voip *)source;

	g_list_free_full(jsvoip->events, g_free);
}

static GSourceFuncs js_voip_source_funcs = {
	.prepare = js_voip_source_prepare,
	.check = js_voip_source_check,
	.dispatch = js_voip_source_dispatch,
	.finalize = js_voip_source_finalize,
};

static JSValueRef js_voip_get_login_state(
	JSContextRef context, JSObjectRef object,
	JSStringRef name, JSValueRef *exception)
{
	struct js_voip *jsvoip = JSObjectGetPrivate(object);
	enum voip_login_state state;
	int err;

	if (!jsvoip) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	err = voip_get_login_state(jsvoip->voip, &state);
	if (err) {
		javascript_set_exception_text(context, exception,
			"failed to get VoIP login state");
		return NULL;
	}

	return javascript_enum_to_string(
		context, voip_login_state_enum, state, exception);
}

static JSValueRef js_voip_get_contact_name(
	JSContextRef context, JSObjectRef object,
	JSStringRef name, JSValueRef *exception)
{
	struct js_voip *jsvoip = JSObjectGetPrivate(object);
	const char *contact = NULL;
	int err;

	if (!jsvoip) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	err = voip_get_contact(jsvoip->voip, &contact, NULL);
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
	struct js_voip *jsvoip = JSObjectGetPrivate(object);
	const char *contact = NULL;
	int err;

	if (!jsvoip) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	err = voip_get_contact(jsvoip->voip, NULL, &contact);
	if (err) {
		javascript_set_exception_text(context, exception,
			"failed to get contact");
		return NULL;
	}

	return javascript_make_string(context, contact, exception);
}

static JSValueRef js_voip_get_onstatechange(JSContextRef context,
		JSObjectRef object, JSStringRef name, JSValueRef *exception)
{
	struct js_voip *jsvoip = JSObjectGetPrivate(object);
	if (!jsvoip) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	if (!jsvoip->state_change_cb)
		return JSValueMakeNull(context);

	return jsvoip->state_change_cb;
}

static bool js_voip_set_onstatechange(JSContextRef context, JSObjectRef object,
		JSStringRef name, JSValueRef value, JSValueRef *exception)
{
	struct js_voip *jsvoip = JSObjectGetPrivate(object);
	int err;

	if (!jsvoip) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return false;
	}

	if (jsvoip->state_change_cb)
		JSValueUnprotect(jsvoip->context, jsvoip->state_change_cb);

	jsvoip->state_change_cb = NULL;

	if (JSValueIsNull(context, value)) {
		voip_set_onstatechange_cb(jsvoip->voip, NULL, NULL,
			(void *)jsvoip->context);
		return true;
	}

	jsvoip->state_change_cb = JSValueToObject(context, value, exception);
	if (!jsvoip->state_change_cb) {
		javascript_set_exception_text(context, exception,
			"failed to assign JS callback");
		return false;
	}
	JSValueProtect(jsvoip->context, jsvoip->state_change_cb);

	err = voip_set_onstatechange_cb(jsvoip->voip, js_voip_state_changed_cb,
		jsvoip, (void *)jsvoip->context);

	if (err) {
		javascript_set_exception_text(context, exception,
			"failed to assign onStateChange callback");
		return false;
	}

	return true;
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
	{
		.name = "onStateChange",
		.getProperty = js_voip_get_onstatechange,
		.setProperty = js_voip_set_onstatechange,
		.attributes = kJSPropertyAttributeDontDelete,
	},
	{}
};

static JSValueRef js_voip_login(
	JSContextRef context, JSObjectRef function, JSObjectRef object,
	size_t argc, const JSValueRef argv[], JSValueRef *exception)
{
	struct js_voip *jsvoip = JSObjectGetPrivate(object);
	int transport = VOIP_TRANSPORT_UDP;
	char *username = NULL;
	char *password = NULL;
	char *server = NULL;
	int port;
	int err;

	if (!jsvoip) {
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

	err = voip_login(jsvoip->voip, server, port, username, password,
		transport);
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
	struct js_voip *jsvoip = JSObjectGetPrivate(object);
	int err;

	if (!jsvoip) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	if (argc > 0) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_ARG_COUNT);
		return NULL;
	}

	err = voip_logout(jsvoip->voip);
	if (err)
		javascript_set_exception_text(context, exception,
			"VoIP logout failed");

	return NULL;
}

static JSValueRef js_voip_call(
	JSContextRef context, JSObjectRef function, JSObjectRef object,
	size_t argc, const JSValueRef argv[], JSValueRef *exception)
{
	struct js_voip *jsvoip = JSObjectGetPrivate(object);
	char *url;
	int err;

	if (!jsvoip) {
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

	err = voip_call(jsvoip->voip, url);
	if (err)
		javascript_set_exception_text(context, exception,
			"VoIP call failed");

	g_free(url);

	return NULL;
}

static JSValueRef js_voip_accept(
	JSContextRef context, JSObjectRef function, JSObjectRef object,
	size_t argc, const JSValueRef argv[], JSValueRef *exception)
{
	struct js_voip *jsvoip = JSObjectGetPrivate(object);
	char *caller = NULL;
	JSValueRef ret;
	int err;

	if (!jsvoip) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	if (argc > 0) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_ARG_COUNT);
		return NULL;
	}

	err = voip_accept(jsvoip->voip, &caller);
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
	struct js_voip *jsvoip = JSObjectGetPrivate(object);
	int err;

	if (!jsvoip) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	if (argc > 0) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_ARG_COUNT);
		return NULL;
	}

	err = voip_terminate(jsvoip->voip);
	if (err)
		javascript_set_exception_text(context, exception,
			"VoIP terminate failed");

	return NULL;
}

static JSValueRef js_voip_dial(
	JSContextRef context, JSObjectRef function, JSObjectRef object,
	size_t argc, const JSValueRef argv[], JSValueRef *exception)
{
	struct js_voip *jsvoip = JSObjectGetPrivate(object);
	char *digits;
	int err;
	int i;

	if (!jsvoip) {
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

		err = voip_dial(jsvoip->voip, digits[i]);
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

static void js_voip_initialize(JSContextRef context, JSObjectRef object)
{
	struct js_voip *jsvoip = JSObjectGetPrivate(object);

	jsvoip->this = object;
}

static void js_voip_finalize(JSObjectRef object)
{
	void *cb_owner;
	struct js_voip *jsvoip = JSObjectGetPrivate(object);

	JSObjectSetPrivate(object, NULL);

	if (jsvoip->voip) {
		cb_owner = voip_get_onstatechange_cb_owner(jsvoip->voip);
		if (cb_owner == (void *)jsvoip->context) {
			voip_set_onstatechange_cb(jsvoip->voip, NULL, NULL,
				cb_owner);
		}
		JSValueUnprotect(jsvoip->context, jsvoip->state_change_cb);
	}
	g_source_destroy(&jsvoip->source);
}

static const JSClassDefinition voip_classdef = {
	.className = "VoIP",
	.initialize = js_voip_initialize,
	.finalize = js_voip_finalize,
	.staticValues = voip_properties,
	.staticFunctions = voip_functions,
};

static JSObjectRef javascript_voip_create(
	JSContextRef js, JSClassRef class,
	struct javascript_userdata *user_data)
{
	struct js_voip *jsvoip;
	GSource *source;

	if (!user_data->rcd || !user_data->rcd->rc)
		return NULL;

	source = g_source_new(&js_voip_source_funcs, sizeof(*jsvoip));
	jsvoip = (struct js_voip *)source;
	if (!jsvoip) {
		g_warning("js-voip: failed to allocate memory");
		return NULL;
	}

	jsvoip->voip = remote_control_get_voip(user_data->rcd->rc);
	if (!jsvoip->voip) {
		g_source_destroy(source);
		return NULL;
	}

	jsvoip->state_change_cb = NULL;
	jsvoip->context = js;

	g_source_attach(source, g_main_loop_get_context(user_data->loop));

	return JSObjectMake(js, class, jsvoip);
}

struct javascript_module javascript_voip = {
	.classdef = &voip_classdef,
	.create = javascript_voip_create,
};
