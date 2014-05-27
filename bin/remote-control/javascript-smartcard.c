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
#include <limits.h>

#include "remote-control-data.h"
#include "remote-control.h"
#include "javascript.h"

/* Only allow transactions with up to 1MB of data */
#define MAX_BUFFER_SIZE (1024*1024)
#define MAX_OFFSET INT_MAX

#define SMARTCARD_TYPE(v, n) { .value = SMARTCARD_TYPE_##v, .name = n }

static const struct javascript_enum smartcard_type_enum[] = {
	SMARTCARD_TYPE(UNKNOWN, "unknown"),
	SMARTCARD_TYPE(I2C, "I2C"),
	{}
};

static JSValueRef js_smartcard_get_type(
	JSContextRef js, JSObjectRef object,
	JSStringRef name, JSValueRef *exception)
{
	struct smartcard *smartcard = JSObjectGetPrivate(object);
	enum smartcard_type type;
	int err;

	if (!smartcard) {
		javascript_set_exception_text(js, exception,
			"object not valid, context switched?");
		return NULL;
	}

	err = smartcard_get_type(smartcard, &type);
	if (err) {
		javascript_set_exception_text(js, exception,
			"failed to get smartcard type");
		return NULL;
	}

	return javascript_enum_to_string(
		js, smartcard_type_enum, type, exception);
}

static const JSStaticValue smartcard_properties[] = {
	{ /* The current audio state as a String */
		.name = "type",
		.getProperty = js_smartcard_get_type,
		.attributes = kJSPropertyAttributeDontDelete |
			kJSPropertyAttributeReadOnly,
	},
	{}
};

static JSValueRef js_smartcard_read(
	JSContextRef js, JSObjectRef function, JSObjectRef object,
	size_t argc, const JSValueRef argv[], JSValueRef *exception)
{
	struct smartcard *smartcard = JSObjectGetPrivate(object);
	JSValueRef array;
	int size, offset = 0;
	char *data;
	int err;

	if (!smartcard) {
		javascript_set_exception_text(js, exception,
			"object not valid, context switched?");
		return NULL;
	}

	if (argc < 1) {
		javascript_set_exception_text(js, exception,
			"invalid arguments count");
		return NULL;
	}

	err = javascript_int_from_number(
		js, argv[0], 0, MAX_BUFFER_SIZE, &size, exception);
	if (err)
		return NULL;

	if (argc > 1) {
		err = javascript_int_from_number(
			js, argv[1], 0, MAX_OFFSET, &offset, exception);
		if (err)
			return NULL;
	}

	data = g_malloc(size);
	if (!data) {
		javascript_set_exception_text(js, exception,
			"failed to allocate data buffer");
		return NULL;
	}

	err = smartcard_read(smartcard, offset, data, size);
	if (err < 0) {
		g_free(data);
		javascript_set_exception_text(js, exception,
			"failed to read smartcard");
		return NULL;
	}

	array = javascript_buffer_to_object(js, data, err, exception);
	g_free(data);

	return array;
}

static JSValueRef js_smartcard_write(
	JSContextRef js, JSObjectRef function, JSObjectRef object,
	size_t argc, const JSValueRef argv[], JSValueRef *exception)
{
	struct smartcard *smartcard = JSObjectGetPrivate(object);
	int size, offset = 0;
	char *data = NULL;
	int err;

	if (!smartcard) {
		javascript_set_exception_text(js, exception,
			"object not valid, context switched?");
		return NULL;
	}

	if (argc < 1) {
		javascript_set_exception_text(js, exception,
			"invalid arguments count");
		return NULL;
	}

	if (argc > 1) {
		err = javascript_int_from_number(
			js, argv[1], 0, MAX_OFFSET, &offset, exception);
		if (err)
			return NULL;
	}

	size = javascript_buffer_from_value(js, argv[0], &data, exception);
	if (size < 0)
		return NULL;

	size = smartcard_write(smartcard, offset, data, size);
	if (size < 0)
		javascript_set_exception_text(js, exception,
			"failed to write smartcard");

	g_free(data);
	return JSValueMakeNumber(js, size);
}

static const JSStaticFunction smartcard_functions[] = {
	{
		.name = "read",
		.callAsFunction = js_smartcard_read,
		.attributes = kJSPropertyAttributeDontDelete,
	},
	{
		.name = "write",
		.callAsFunction = js_smartcard_write,
		.attributes = kJSPropertyAttributeDontDelete,
	},
	{}
};

static JSClassDefinition smartcard_classdef = {
	.className = "SmartCard",
	.staticValues = smartcard_properties,
	.staticFunctions = smartcard_functions,
};

static JSObjectRef javascript_smartcard_create(
	JSContextRef js, JSClassRef class,
	struct javascript_userdata *user_data)
{
	struct smartcard *smartcard;

	if (!user_data->rcd || !user_data->rcd->rc)
		return NULL;

	smartcard = remote_control_get_smartcard(user_data->rcd->rc);
	if (!smartcard)
		return NULL;

	return JSObjectMake(js, class, smartcard);
}

struct javascript_module javascript_smartcard = {
	.classdef = &smartcard_classdef,
	.create = javascript_smartcard_create,
};
