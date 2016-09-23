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

struct js_datagram {
	GSource source;
	struct net_udp *net_udp;
	JSContextRef context;
	JSObjectRef recv_cb;
	JSObjectRef this;
	GList *events;
};

static void js_datagram_event_cb(int channel, void *data)
{
	struct js_datagram *jsdg = data;
	int *evt;

	if (!jsdg)
		return;

	evt = g_new0(int, 1);
	if (!evt)
		return;

	*evt = channel;
	jsdg->events = g_list_append(jsdg->events, evt);
}

static void js_datagram_send_event(struct js_datagram *jsdg, int *channel)
{
	JSValueRef exception = NULL;
	JSValueRef args[1];

	args[0] = JSValueMakeNumber(jsdg->context, *channel);

	(void)JSObjectCallAsFunction(jsdg->context, jsdg->recv_cb, jsdg->this,
			G_N_ELEMENTS(args), args, &exception);

	if (exception)
		g_warning(JS_LOG_CALLBACK_EXCEPTION, __func__);
}

static gboolean js_datagram_source_prepare(GSource *source, gint *timeout)
{
	if (timeout)
		*timeout = 100;

	return FALSE;
}

static gboolean js_datagram_source_check(GSource *source)
{
	struct js_datagram *jsdg = (struct js_datagram *)source;

	if (jsdg && g_list_first(jsdg->events))
		return TRUE;

	return FALSE;
}
static gboolean js_datagram_source_dispatch(GSource *source,
		GSourceFunc callback, gpointer user_data)
{
	struct js_datagram *jsdg = (struct js_datagram *)source;
	GList *node = jsdg ? g_list_first(jsdg->events) : NULL;
	int *evt = node ? node->data : NULL;

	while (evt) {
		if (jsdg->context && jsdg->recv_cb)
			js_datagram_send_event(jsdg, evt);
		jsdg->events = g_list_remove(jsdg->events, evt);
		g_free(evt);
		node = g_list_first(jsdg->events);
		evt = node ? node->data : NULL;
	}

	if (callback)
		return callback(user_data);

	return TRUE;
}

static void js_datagram_source_finalize(GSource *source)
{
	struct js_datagram *jsdg = (struct js_datagram *)source;

	g_list_free_full(jsdg->events, g_free);
}

static GSourceFuncs js_datagram_source_funcs = {
	.prepare = js_datagram_source_prepare,
	.check = js_datagram_source_check,
	.dispatch = js_datagram_source_dispatch,
	.finalize = js_datagram_source_finalize,
};

static JSValueRef js_datagram_get_on_receive(JSContextRef context,
		JSObjectRef object, JSStringRef name, JSValueRef *exception)
{
	struct js_datagram *jsdg = JSObjectGetPrivate(object);

	if (!jsdg) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	return jsdg->recv_cb ? JSValueMakeNull(context) : jsdg->recv_cb;
}

static bool js_datagram_set_on_receive(JSContextRef context, JSObjectRef object,
		JSStringRef name, JSValueRef value, JSValueRef *exception)
{
	struct js_datagram *jsdg = JSObjectGetPrivate(object);

	if (!jsdg) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return false;
	}

	if (jsdg->recv_cb)
		JSValueUnprotect(jsdg->context, jsdg->recv_cb);

	if (JSValueIsNull(context, value)) {
		void *owner = net_udp_get_recv_cb_owner(jsdg->net_udp);

		if ((void *)jsdg->context == owner)
			net_udp_set_recv_cb(jsdg->net_udp, NULL, NULL,
				(void *)jsdg->context);
		jsdg->recv_cb = NULL;
		return true;
	}

	jsdg->recv_cb = JSValueToObject(context, value, exception);
	if (!jsdg->recv_cb) {
		javascript_set_exception_text(context, exception,
			"failed to assign JS callback");
		return false;
	}
	JSValueProtect(context, jsdg->recv_cb);

	if (net_udp_set_recv_cb(jsdg->net_udp, js_datagram_event_cb,
			jsdg, (void *)jsdg->context)) {
		javascript_set_exception_text(context, exception,
			"failed to assign onReceive callback");
		return false;
	}

	return true;
}

static const JSStaticValue datagram_properties[] = {
	{
		.name = "onReceive",
		.getProperty = js_datagram_get_on_receive,
		.setProperty = js_datagram_set_on_receive,
		.attributes = kJSPropertyAttributeNone,
	},
	{}
};

static JSValueRef datagram_function_create_channel(JSContextRef js,
		JSObjectRef function, JSObjectRef object, size_t argc,
		const JSValueRef argv[], JSValueRef *exception)
{
	struct js_datagram *jsdg = JSObjectGetPrivate(object);
	int port_remote;
	int port_local;
	char *hostname;
	int ret;

	if (!jsdg) {
		javascript_set_exception_text(js, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	if (argc != 3) {
		javascript_set_exception_text(js, exception,
			"invalid arguments count: "
			"use 'hostname, remote_port, local_port'");
		return NULL;
	}

	ret = javascript_int_from_number(js, argv[1], 0, UINT16_MAX,
		&port_remote, exception);
	if (ret)
		return NULL;


	ret = javascript_int_from_number(js, argv[2], 0, UINT16_MAX,
		&port_local, exception);
	if (ret)
		return NULL;

	hostname = javascript_get_string(js, argv[0], exception);
	if (!hostname)
		return NULL;

	ret = net_udp_create_channel(jsdg->net_udp, port_local, hostname,
		port_remote);
	if (ret < 0) {
		javascript_set_exception_text(js, exception,
			"Creating channel failed: %d", ret);
	}

	g_free(hostname);

	return ret >= 0 ? JSValueMakeNumber(js, ret) : NULL;
}

static JSValueRef datagram_function_destroy_channel(JSContextRef js,
		JSObjectRef function, JSObjectRef object, size_t argc,
		const JSValueRef argv[], JSValueRef *exception)
{
	struct js_datagram *jsdg = JSObjectGetPrivate(object);
	int cref;
	int ret;

	if (!jsdg) {
		javascript_set_exception_text(js, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	if (argc != 1) {
		javascript_set_exception_text(js, exception,
			"invalid arguments count: use 'channel_ref'");
		return NULL;
	}

	ret = javascript_int_from_number(js, argv[0], 0, UINT16_MAX, &cref,
		exception);
	if (ret)
		return NULL;

	ret = net_udp_destroy_channel(jsdg->net_udp, cref);
	if (ret)
		javascript_set_exception_text(js, exception,
			"Destroying channel failed: %d", ret);

	return JSValueMakeNull(js);
}

static JSValueRef datagram_function_send(JSContextRef js, JSObjectRef function,
		JSObjectRef object, size_t argc, const JSValueRef argv[],
		JSValueRef *exception)
{
	struct js_datagram *jsdg = JSObjectGetPrivate(object);
	struct net_udp_channel *channel;
	char *data = NULL;
	int cref;
	int size;
	int ret;

	if (!jsdg) {
		javascript_set_exception_text(js, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	if (argc != 2) {
		javascript_set_exception_text(js, exception,
			"invalid arguments count: use 'channel_ref, data[]'");
		return NULL;
	}

	ret = javascript_int_from_number(js, argv[0], 0, UINT16_MAX, &cref,
		exception);
	if (ret)
		return NULL;

	channel = net_udp_get_channel_by_ref(jsdg->net_udp, cref);
	if (!channel) {
		javascript_set_exception_text(js, exception,
			"no channel for this reference");
		return NULL;
	}

	size = javascript_buffer_from_value(js, argv[1], &data, exception);
	if (size < 0)
		return NULL;

	if (!size)
		return JSValueMakeNumber(js, 0);

	ret = net_udp_send(channel, data, size);
	if (ret < 0)
		javascript_set_exception_text(js, exception,
			"failed to send datagram: %d", ret);

	g_free(data);

	return ret < 0 ? NULL : JSValueMakeNumber(js, ret);
}

static JSValueRef datagram_function_recv(JSContextRef js, JSObjectRef function,
		JSObjectRef object, size_t argc, const JSValueRef argv[],
		JSValueRef *exception)
{
	struct js_datagram *jsdg = JSObjectGetPrivate(object);
	int size = NET_UDP_MAX_PACKET_SIZE;
	struct net_udp_channel *channel;
	JSValueRef array;
	char *data;
	int cref;
	int ret;

	if (!jsdg) {
		javascript_set_exception_text(js, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	if (argc < 1 || argc > 2) {
		javascript_set_exception_text(js, exception,
			"invalid arguments count: use 'channel_ref [, size]'");
		return NULL;
	}

	ret = javascript_int_from_number(js, argv[0], 0, UINT16_MAX, &cref,
		exception);
	if (ret)
		return NULL;

	channel = net_udp_get_channel_by_ref(jsdg->net_udp, cref);
	if (!channel) {
		javascript_set_exception_text(js, exception,
			"no channel for this reference");
		return NULL;
	}

	if (argc == 2) {
		ret = javascript_int_from_number(js, argv[1], 0,
			NET_UDP_MAX_PACKET_SIZE, &size, exception);
		if (ret)
			return NULL;

		if (!size) {
			return javascript_buffer_to_object(js, NULL, 0,
				exception);
		}
	}

	data = g_malloc(size);
	if (!data) {
		javascript_set_exception_text(js, exception,
			"failed to allocate data buffer");
		return NULL;
	}

	ret = net_udp_recv(channel, data, size);
	if (ret < 0) {
		g_free(data);
		javascript_set_exception_text(js, exception,
			"failed to fetch datagram: %d", ret);
		return NULL;
	}

	array = javascript_buffer_to_object(js, data, ret, exception);
	g_free(data);

	return array;
}

static const JSStaticFunction datagram_functions[] = {
	{
		.name = "createChannel",
		.callAsFunction = datagram_function_create_channel,
		.attributes = kJSPropertyAttributeNone,
	},
	{
		.name = "destroyChannel",
		.callAsFunction = datagram_function_destroy_channel,
		.attributes = kJSPropertyAttributeNone,
	},
	{
		.name = "send",
		.callAsFunction = datagram_function_send,
		.attributes = kJSPropertyAttributeNone,
	},
	{
		.name = "receive",
		.callAsFunction = datagram_function_recv,
		.attributes = kJSPropertyAttributeNone,
	},
	{}
};


static void datagram_initialize(JSContextRef context, JSObjectRef object)
{
	struct js_datagram *jsdg = JSObjectGetPrivate(object);

	jsdg->this = object;
}

static void datagram_finalize(JSObjectRef object)
{
	struct js_datagram *jsdg = JSObjectGetPrivate(object);
	void *cb_owner;

	if (jsdg->recv_cb) {
		cb_owner = net_udp_get_recv_cb_owner(jsdg->net_udp);
		if ((void *)jsdg->context == cb_owner) {
			net_udp_set_recv_cb(jsdg->net_udp, NULL, NULL,
				(void *)jsdg->context);
		}
		JSValueUnprotect(jsdg->context, jsdg->recv_cb);
	}
	g_source_destroy(&jsdg->source);
}

static const JSClassDefinition datagram_classdef = {
	.className = "Datagram",
	.initialize = datagram_initialize,
	.finalize = datagram_finalize,
	.staticValues = datagram_properties,
	.staticFunctions = datagram_functions,
};

static JSObjectRef javascript_datagram_create(JSContextRef js, JSClassRef class,
		struct javascript_userdata *user_data)
{
	struct js_datagram *jsdg;
	GSource *source;

	if (!user_data->rcd || !user_data->rcd->rc)
		return NULL;

	source = g_source_new(&js_datagram_source_funcs, sizeof(*jsdg));
	jsdg = (struct js_datagram *)source;
	if (!jsdg) {
		g_warning("js-datagram: failed to allocate memory");
		return NULL;
	}

	jsdg->net_udp = remote_control_get_net_udp(user_data->rcd->rc);
	if (!jsdg->net_udp) {
		g_source_destroy(source);
		return NULL;
	}

	jsdg->recv_cb = NULL;
	jsdg->context = js;

	g_source_attach(source, g_main_loop_get_context(user_data->loop));

	return JSObjectMake(js, class, jsdg);
}

struct javascript_module javascript_datagram = {
	.classdef = &datagram_classdef,
	.create = javascript_datagram_create,
};
