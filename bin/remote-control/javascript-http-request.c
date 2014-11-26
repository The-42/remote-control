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
#include <string.h>
#include <libsoup/soup.h>
#include <nettle/blowfish.h>
#include <nettle/base16.h>

#include "javascript.h"

struct http_request {
	SoupSession *session;
};

static int http_request_encrypt(JSContextRef context, struct blowfish_ctx *ctx,
	char **cmd, JSValueRef *exception)
{
	size_t len = ((strlen(*cmd) + BLOWFISH_BLOCK_SIZE - 1) /
			BLOWFISH_BLOCK_SIZE) * BLOWFISH_BLOCK_SIZE;
	uint8_t *crypt = g_new0(uint8_t, len);
	int ret = 0;

	if (!strlen(*cmd))
		goto cleanup;
	if (!crypt) {
		ret = -ENOMEM;
		javascript_set_exception_text(context, exception,
				"Out of memory");
		goto cleanup;
	}
	blowfish_encrypt(ctx, len, crypt, (uint8_t *)*cmd);
	g_free(*cmd);
	*cmd = g_new0(char, BASE16_ENCODE_LENGTH(len));
	if (!*cmd) {
		ret = -ENOMEM;
		javascript_set_exception_text(context, exception,
				"Out of memory");
		goto cleanup;
	}
	base16_encode_update((uint8_t *)*cmd, len, crypt);

cleanup:
	if (crypt)
		g_free(crypt);
	return ret;
}

static JSValueRef http_request_decrypt(JSContextRef context,
	struct blowfish_ctx *ctx, const char *cmd, JSValueRef *exception)
{
	size_t src_len = strlen(cmd);
	size_t dst_len = BASE16_DECODE_LENGTH(src_len);
	uint8_t *crypt = g_new0(uint8_t, dst_len);
	struct base16_decode_ctx base16_ctx;
	JSValueRef ret = NULL;
	char *data = NULL;

	if (!crypt) {
		javascript_set_exception_text(context, exception,
				"Out of memory");
		goto cleanup;
	}

	base16_decode_init(&base16_ctx);
	base16_decode_update(&base16_ctx, &dst_len, crypt, src_len,
			(uint8_t *)cmd);
	if (!base16_decode_final(&base16_ctx)) {
		javascript_set_exception_text(context, exception,
				"Failed to decode");
		goto cleanup;
	}
	if (dst_len % BLOWFISH_BLOCK_SIZE) {
		javascript_set_exception_text(context, exception,
				"Invalid data size: %d", dst_len);
		goto cleanup;
	}
	data = g_new0(char, dst_len);
	if (!data) {
		javascript_set_exception_text(context, exception,
				"Out of memory");
		goto cleanup;
	}
	blowfish_decrypt(ctx, dst_len, (uint8_t *)data, crypt);
	ret = javascript_make_string( context, data, exception);

cleanup:
	if (crypt)
		g_free(crypt);
	if (data)
		g_free(data);
	return ret;
}

static JSValueRef http_request_function_send(
	JSContextRef context, JSObjectRef function, JSObjectRef object,
	size_t argc, const JSValueRef argv[], JSValueRef *exception)
{
	struct http_request *req = JSObjectGetPrivate(object);
	struct blowfish_ctx ctx;
	const char *method = "GET";
	SoupMessage *msg = NULL;
	JSValueRef ret = NULL;
	char *key = NULL;
	char *cmd = NULL;
	char *uri = NULL;
	guint status;

	if (!req) {
		javascript_set_exception_text(context, exception,
				JS_ERR_INVALID_OBJECT_TEXT);
		goto cleanup;
	}
	/* Usage: send(uri[,request[,blowfish key]]) */
	switch (argc) {
	case 3:
		key = javascript_get_string(context, argv[2], exception);
		if (!key)
			goto cleanup;
		blowfish_set_key(&ctx,
				MIN(BLOWFISH_MAX_KEY_SIZE, strlen(key)),
				(uint8_t *)key);
		/* no break */
	case 2:
		cmd = javascript_get_string(context, argv[1], exception);
		if (!cmd)
			goto cleanup;
		method = "POST";
		/* no break */
	case 1:
		uri = javascript_get_string(context, argv[0], exception);
		if (!uri)
			goto cleanup;
		break;
	default:
		javascript_set_exception_text(context, exception,
				JS_ERR_INVALID_ARG_COUNT);
		goto cleanup;
	}

	msg = soup_message_new(method, uri);
	if (!msg) {
		javascript_set_exception_text(context, exception,
				"Failed to create message");
		goto cleanup;
	}
	if (cmd) {
		if (key && http_request_encrypt(context, &ctx, &cmd, exception))
			goto cleanup;
		soup_message_set_request(msg, "text/html; charset=utf-8",
				SOUP_MEMORY_COPY, cmd, strlen(cmd));
	}
	status = soup_session_send_message(req->session, msg);

	if (msg->response_body && msg->response_body->length) {
		if (key)
			ret = http_request_decrypt(context, &ctx,
					msg->response_body->data, exception);
		else
			ret = javascript_make_string( context,
					msg->response_body->data, exception);
	} else if (!SOUP_STATUS_IS_SUCCESSFUL(status)) {
		javascript_set_exception_text(context, exception,
				"Message not successful: (%d) %s",
				status, soup_status_get_phrase(status));
		goto cleanup;
	} else {
		ret = JSValueMakeNull(context);
	}

cleanup:
	if (msg)
		g_object_unref(msg);
	if (key)
		g_free(key);
	if (cmd)
		g_free(cmd);
	if (uri)
		g_free(uri);
	return ret;
}

static JSValueRef http_request_function_set_timeout(
	JSContextRef context, JSObjectRef function, JSObjectRef object,
	size_t argc, const JSValueRef argv[], JSValueRef *exception)
{
	struct http_request *req = JSObjectGetPrivate(object);
	GValue val = G_VALUE_INIT;
	JSValueRef ret = NULL;
	int timeout = 0;

	if (!req) {
		javascript_set_exception_text(context, exception,
				JS_ERR_INVALID_OBJECT_TEXT);
		goto cleanup;
	}
	/* Usage: setTimeout(timeout) */
	if (argc != 1) {
		javascript_set_exception_text(context, exception,
				"Failed to create message");
		goto cleanup;
	}
	if (javascript_int_from_number(context, argv[0], 0,
			UINT16_MAX, &timeout, exception)) {
		javascript_set_exception_text(context, exception,
				JS_ERR_INVALID_NUMBER);
		goto cleanup;
	}
	g_value_init(&val, G_TYPE_INT);
	g_value_set_int(&val, timeout);
	g_object_set_property(G_OBJECT(req->session), "timeout", &val);

cleanup:
	return ret;
}

static struct http_request *http_request_new(JSContextRef context,
	struct javascript_userdata *data)
{
	struct http_request *req = g_new0(struct http_request, 1);

	if (!req) {
		g_warning("js-request: failed to allocate memory");
		return NULL;
	}
	req->session = soup_session_new();
	if (!req->session)
		goto cleanup;

	return req;

cleanup:
	g_free(req);
	return NULL;

}

static void http_request_finalize(JSObjectRef object)
{
	struct http_request *req = JSObjectGetPrivate(object);

	if (!req)
		return;

	g_clear_object(&req->session);
	g_free(req);
}

static const JSStaticFunction http_request_functions[] = {
	{
		.name = "send",
		.callAsFunction = http_request_function_send,
		.attributes = kJSPropertyAttributeDontDelete,
	},{
		.name = "setTimeout",
		.callAsFunction = http_request_function_set_timeout,
		.attributes = kJSPropertyAttributeDontDelete,
	},{
	}
};

static const JSClassDefinition http_request_classdef = {
	.className = "HTTPRequest",
	.finalize = http_request_finalize,
	.staticFunctions = http_request_functions,
};

static JSObjectRef javascript_http_request_create(
	JSContextRef js, JSClassRef class,
	struct javascript_userdata *user_data)
{
	struct http_request *req;

	req = http_request_new(js, user_data);
	if (!req)
		return NULL;

	return JSObjectMake(js, class, req);
}

struct javascript_module javascript_http_request = {
	.classdef = &http_request_classdef,
	.create = javascript_http_request_create,
};
