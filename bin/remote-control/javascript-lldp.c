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
