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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "javascript.h"

static JSValueRef js_lldp_get_info(JSContextRef js, JSObjectRef object,
		JSStringRef name, JSValueRef *exception)
{
	struct lldp_monitor *lldp = JSObjectGetPrivate(object);
	GHashTableIter iter;
	GHashTable *info;
	JSObjectRef ret;
	char *key, *val;
	int err;

	if ((err = lldp_monitor_read_info(lldp, &info)) < 0) {
		*exception = JSValueMakeNumber(js, err);
		return NULL;
	}

	ret = JSObjectMake(js, NULL, NULL);

	g_hash_table_iter_init(&iter, info);
	while (g_hash_table_iter_next(&iter, (gpointer)&key, (gpointer)&val)) {
		if (!val)
			continue;
		javascript_object_set_property(js, ret, key,
				javascript_make_string(js, val, NULL),
				0, NULL);
	}

	g_hash_table_unref(info);

	return ret;
}

static const JSStaticValue lldp_properties[] = {
	{
		.name = "info",
		.getProperty = js_lldp_get_info,
		.attributes = kJSPropertyAttributeDontDelete |
			kJSPropertyAttributeReadOnly,
	},
	{}
};

static JSValueRef js_lldp_read(JSContextRef js, JSObjectRef function,
		JSObjectRef object, size_t argc, const JSValueRef argv[],
		JSValueRef *exception)
{
	struct lldp_monitor *lldp = JSObjectGetPrivate(object);
	int size = LLDP_MAX_SIZE;
	JSValueRef array;
	char *data;
	int err;

	if (!lldp) {
		javascript_set_exception_text(js, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return JSValueMakeNull(js);
	}

	if (argc > 1) {
		javascript_set_exception_text(js, exception,
			"invalid arguments count: use '[size]'");
		return JSValueMakeNull(js);
	}

	if (argc > 0) {
		err = javascript_int_from_number(
			js, argv[0], 0, LLDP_MAX_SIZE, &size, exception);

		if (err || !size)
			return JSValueMakeNull(js);
	}

	data = g_malloc(size);
	if (!data) {
		javascript_set_exception_text(js, exception,
			"failed to allocate data buffer");
		return JSValueMakeNull(js);
	}

	g_debug("lldp_monitor_read(%p, %p, %d)", lldp, data, size);
	err = lldp_monitor_read(lldp, data, size);
	if (err < 0) {
		g_free(data);
		javascript_set_exception_text(js, exception,
			"failed to read LLDP data");
		return JSValueMakeNull(js);
	}

	array = javascript_buffer_to_object(js, data, err, exception);
	g_free(data);

	return array;
}

static const JSStaticFunction lldp_functions[] = {
	{
		.name = "dump",
		.callAsFunction = js_lldp_read,
		.attributes = kJSPropertyAttributeDontDelete,
	}, {
	}
};

static const JSClassDefinition lldp_classdef = {
	.className = "LLDP",
	.staticValues = lldp_properties,
	.staticFunctions = lldp_functions,
};

static JSObjectRef javascript_lldp_create(JSContextRef js, JSClassRef class,
		struct javascript_userdata *user_data)
{
	struct lldp_monitor *lldp;

	if (!user_data->rcd || !user_data->rcd->rc)
		return NULL;

	lldp = remote_control_get_lldp_monitor(user_data->rcd->rc);
	if (!lldp)
		return NULL;

	return JSObjectMake(js, class, lldp);
}

struct javascript_module javascript_lldp = {
	.classdef = &lldp_classdef,
	.create = javascript_lldp_create,
};
