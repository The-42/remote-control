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

struct monitor {
	/* if it is empty, can we remove it? */
	int dummy;
};

static JSValueRef monitor_function_get_free_memory(
	JSContextRef context, JSObjectRef function, JSObjectRef object,
	size_t argc, const JSValueRef argv[], JSValueRef *exception)
{
	struct monitor *priv = JSObjectGetPrivate(object);
	long avail_pages, page_size;

	if (!priv) {
		javascript_set_exception_text(context, exception,
			"object notvalid, context switched?");
		return JSValueMakeNumber(context, -1);
	}

	if (argc != 0) {
		javascript_set_exception_text(context, exception,
			"invalid argument count");
		return JSValueMakeNumber(context, -1);
	}

	/* TODO: mayby we should and query the memory usage of
	 *       remote-control and its children to calculate
	 *       the free memory? */
	avail_pages = sysconf(_SC_AVPHYS_PAGES);
	page_size = sysconf(_SC_PAGE_SIZE);

	if (avail_pages < 0 || page_size < 0) {
		javascript_set_exception_text(context, exception,
			"failed to query memory stats");
		return JSValueMakeNumber(context, -1);
	}

	return JSValueMakeNumber(context, (avail_pages * page_size) / 1024);
}

static struct monitor *monitor_new(JSContextRef context,
	struct javascript_userdata *data)
{
	struct monitor *mon;

	mon = g_new0(struct monitor, 1);
	if (!mon) {
		g_warning("js-monitor: failed to allocate memory");
		return NULL;
	}

	return mon;
}

static void monitor_finalize(JSObjectRef object)
{
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
	.finalize = monitor_finalize,
	.staticFunctions = monitor_functions,
};

static JSClassRef monitor_class = NULL;

int javascript_register_monitor_class(void)
{
	monitor_class = JSClassCreate(&monitor_classdef);
	if (!monitor_class) {
		g_warning("js-monitor: failed to create Monitor class");
		return -ENOMEM;
	}

	return 0;
}

int javascript_register_monitor(JSContextRef js,
	JSObjectRef parent, const char *name, void *user_data)
{
	JSValueRef exception = NULL;
	JSObjectRef object;
	JSStringRef string;
	struct monitor *mon;

	mon = monitor_new(js, user_data);
	if (!mon)
		return -ENOMEM;

	object = JSObjectMake(js, monitor_class, mon);
	string = JSStringCreateWithUTF8CString(name);

	JSObjectSetProperty(js, parent, string, object, 0, &exception);

	JSStringRelease(string);
	return 0;
}
