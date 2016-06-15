/*
 * Copyright (C) 2013 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <unistd.h>
#include <errno.h>

#include "javascript.h"

static JSValueRef monitor_function_get_free_memory(
	JSContextRef context, JSObjectRef function, JSObjectRef object,
	size_t argc, const JSValueRef argv[], JSValueRef *exception)
{
	long avail_pages, page_size;

	if (argc != 0) {
		javascript_set_exception_text(context, exception,
			"invalid argument count");
		return NULL;
	}

	/* TODO: mayby we should and query the memory usage of
	 *       remote-control and its children to calculate
	 *       the free memory? */
	avail_pages = sysconf(_SC_AVPHYS_PAGES);
	page_size = sysconf(_SC_PAGE_SIZE);

	if (avail_pages < 0 || page_size < 0) {
		javascript_set_exception_text(context, exception,
			"failed to query memory stats");
		return NULL;
	}

	return JSValueMakeNumber(context, (avail_pages * page_size) / 1024);
}

static const JSStaticFunction monitor_functions[] = {
	{
		.name = "getFreeMem",
		.callAsFunction = monitor_function_get_free_memory,
		.attributes = kJSPropertyAttributeNone,
	},{
	}
};

static const JSClassDefinition monitor_classdef = {
	.className = "Monitor",
	.staticFunctions = monitor_functions,
};

static JSObjectRef javascript_monitor_create(
	JSContextRef js, JSClassRef class,
	struct javascript_userdata *user_data)
{
	return JSObjectMake(js, class, NULL);
}

struct javascript_module javascript_monitor = {
	.classdef = &monitor_classdef,
	.create = javascript_monitor_create,
};
