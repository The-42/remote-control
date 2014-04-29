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
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "remote-control-data.h"
#include "remote-control.h"
#include "javascript.h"

struct taskmanager {
	struct remote_control_data *rcd;
};

static JSValueRef taskmanager_function_exec(JSContextRef context,
		JSObjectRef function, JSObjectRef object,
		size_t argc, const JSValueRef argv[], JSValueRef *exception)
{
	struct taskmanager *tm = JSObjectGetPrivate(object);
	JSStringRef string;
	char *command;
	size_t length;
	int32_t ret;

	if (!tm) {
		javascript_set_exception_text(context, exception,
			"object not valid, context switched?");
		return JSValueMakeNumber(context, -EINVAL);
	}

	if (argc != 1) {
		javascript_set_exception_text(context, exception,
			"invalid argument count");
		return JSValueMakeNumber(context, -EINVAL);
	}

	if (!JSValueIsString(context, argv[0])) {
		javascript_set_exception_text(context, exception,
			"command is not a string");
		return JSValueMakeNumber(context, -EINVAL);
	}

	string = JSValueToStringCopy(context, argv[0], exception);
	if (!string)
		return JSValueMakeNumber(context, -EINVAL);

	length = JSStringGetMaximumUTF8CStringSize(string);
	command = g_alloca(length + 1);
	length = JSStringGetUTF8CString(string, command, length);
	JSStringRelease(string);

	if (!tm->rcd->rc) {
		javascript_set_exception_text(context, exception,
			"remote-control context not ready");
		return JSValueMakeNumber(context, -EBUSY);
	}

	ret = task_manager_exec(tm->rcd->rc, command);
	if (ret < 0) {
		javascript_set_exception_text(context, exception,
			"command could not be executed");
		return JSValueMakeNumber(context, ret);
	}

	return JSValueMakeNumber(context, ret);
}

static JSValueRef taskmanager_function_kill(JSContextRef context,
		JSObjectRef function, JSObjectRef object,
		size_t argc, const JSValueRef argv[], JSValueRef *exception)
{
	struct taskmanager *tm = JSObjectGetPrivate(object);
	int32_t pid, signal;
	int32_t ret;

	if (!tm) {
		javascript_set_exception_text(context, exception,
			"object not valid, context switched?");
		return JSValueMakeNumber(context, -EINVAL);
	}

	if (argc != 2) {
		javascript_set_exception_text(context, exception,
			"invalid argument count");
		return JSValueMakeNumber(context, -EINVAL);
	}

	if (!JSValueIsNumber(context, argv[0])) {
		javascript_set_exception_text(context, exception,
			"pid is not a number");
		return JSValueMakeNumber(context, -EINVAL);
	}

	if (!JSValueIsNumber(context, argv[1])) {
		javascript_set_exception_text(context, exception,
			"signal is not a number");
		return JSValueMakeNumber(context, -EINVAL);
	}

	if (!tm->rcd->rc) {
		javascript_set_exception_text(context, exception,
			"remote-control context not ready");
		return JSValueMakeNumber(context, -EBUSY);
	}

	pid = JSValueToNumber(context, argv[0], exception);
	signal = JSValueToNumber(context, argv[1], exception);
	ret = task_manager_kill(tm->rcd->rc, pid, signal);
	if (ret < 0) {
		javascript_set_exception_text(context, exception,
			"kill could not be executed");
		return JSValueMakeNumber(context, ret);
	}

	return JSValueMakeNumber(context, ret);
}
static struct taskmanager *taskmanager_new(JSContextRef context,
		struct javascript_userdata *data)
{
	struct taskmanager *tm;

	if (!data->rcd) {
		g_warning("js-taskmanager: No remote-control context provided");
		return NULL;
	}

	tm = g_new0(struct taskmanager, 1);
	if (!tm)
		g_warning("js-taskmanger: failed to allocate memory");

	tm->rcd = data->rcd;

	return tm;
}

static void taskmanager_finalize(JSObjectRef object)
{
	struct taskmanager *tm = JSObjectGetPrivate(object);
	g_free(tm);
}

static const JSStaticFunction taskmanager_functions[] = {
	{
		.name = "exec",
		.callAsFunction = taskmanager_function_exec,
		.attributes = kJSPropertyAttributeNone,
	}, {
		.name = "kill",
		.callAsFunction = taskmanager_function_kill,
		.attributes = kJSPropertyAttributeNone,
	}, {
	}
};

static const JSClassDefinition taskmanager_classdef = {
	.className = "TaskManager",
	.finalize = taskmanager_finalize,
	.staticFunctions = taskmanager_functions,
};

static JSObjectRef javascript_taskmanager_create(
	JSContextRef js, JSClassRef class,
        struct javascript_userdata *user_data)
{
	struct taskmanager *tm;

	tm = taskmanager_new(js, user_data);
	if (!tm)
		return NULL;

	return JSObjectMake(js, class, tm);
}

struct javascript_module javascript_taskmanager = {
	.classdef = &taskmanager_classdef,
	.create = javascript_taskmanager_create,
};
