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
#include <signal.h>

#include "remote-control-data.h"
#include "remote-control.h"
#include "javascript.h"

struct taskmanager {
	struct remote_control_data *rcd;

	JSContextRef context;
	JSObjectRef callback;
};

#define SIG(s) { .name = "SIG" #s, .signo = SIG##s }

static const struct {
	const char *name;
	int signo;
} signal_names[] = {
	SIG(ABRT),
	SIG(ALRM),
	SIG(BUS),
	SIG(CHLD),
	SIG(CONT),
	SIG(FPE),
	SIG(HUP),
	SIG(ILL),
	SIG(INT),
	SIG(KILL),
	SIG(PIPE),
	SIG(QUIT),
	SIG(SEGV),
	SIG(STOP),
	SIG(TERM),
	SIG(TSTP),
	SIG(TTIN),
	SIG(TTOU),
	SIG(USR1),
	SIG(USR2),
	SIG(POLL),
	SIG(PROF),
	SIG(SYS),
	SIG(TRAP),
	SIG(URG),
	SIG(VTALRM),
	SIG(XCPU),
	SIG(XFSZ),
	{}
};

static JSValueRef taskmanager_get_signal(
	JSContextRef context, JSObjectRef object,
	JSStringRef name, JSValueRef *exception)
{
	int i;

	for (i = 0; signal_names[i].name; i++)
		if (JSStringIsEqualToUTF8CString(
				name, signal_names[i].name))
			return JSValueMakeNumber(
				context, signal_names[i].signo);

	return NULL;
}

static JSValueRef taskmanager_get_onevent(JSContextRef context,
		JSObjectRef object, JSStringRef name, JSValueRef *exception)
{
	struct taskmanager *tm = JSObjectGetPrivate(object);
	if (!tm) {
		g_warning("%s: object not valid, context changed?", __func__);
		return JSValueMakeNull(context);
	}

	return tm->callback;
}

static bool taskmanager_set_onevent(JSContextRef context, JSObjectRef object,
		JSStringRef name, JSValueRef value, JSValueRef *exception)
{
	struct taskmanager *tm = JSObjectGetPrivate(object);
	if (!tm) {
		g_warning("%s: object not valid, context changed?", __func__);
		return false;
	}

	if (tm->callback)
		JSValueUnprotect(context, tm->callback);

	if (JSValueIsNull(context, value)) {
		tm->callback = NULL;
		return true;
	}

	tm->callback = JSValueToObject(context, value, exception);
	if (!tm->callback) {
		g_warning("%s: failed to assign callback", __func__);
		return false;
	}
	JSValueProtect(context, tm->callback);
	return true;
}

#define PROPERTY_SIG(s)						\
	{							\
		.name = "SIG" #s,				\
		.getProperty = taskmanager_get_signal,		\
		.attributes = kJSPropertyAttributeDontDelete |	\
			kJSPropertyAttributeReadOnly,		\
	}

static const JSStaticValue taskmanager_properties[] = {
	{
		.name = "onevent",
		.getProperty = taskmanager_get_onevent,
		.setProperty = taskmanager_set_onevent,
		.attributes = kJSPropertyAttributeNone,
	},
	PROPERTY_SIG(ABRT),
	PROPERTY_SIG(ALRM),
	PROPERTY_SIG(BUS),
	PROPERTY_SIG(CHLD),
	PROPERTY_SIG(CONT),
	PROPERTY_SIG(FPE),
	PROPERTY_SIG(HUP),
	PROPERTY_SIG(ILL),
	PROPERTY_SIG(INT),
	PROPERTY_SIG(KILL),
	PROPERTY_SIG(PIPE),
	PROPERTY_SIG(QUIT),
	PROPERTY_SIG(SEGV),
	PROPERTY_SIG(STOP),
	PROPERTY_SIG(TERM),
	PROPERTY_SIG(TSTP),
	PROPERTY_SIG(TTIN),
	PROPERTY_SIG(TTOU),
	PROPERTY_SIG(USR1),
	PROPERTY_SIG(USR2),
	PROPERTY_SIG(POLL),
	PROPERTY_SIG(PROF),
	PROPERTY_SIG(SYS),
	PROPERTY_SIG(TRAP),
	PROPERTY_SIG(URG),
	PROPERTY_SIG(VTALRM),
	PROPERTY_SIG(XCPU),
	PROPERTY_SIG(XFSZ),
	{}
};

static void taskmanager_terminate_cb(int pid, void *data, int status)
{
	JSObjectRef object = data;
	struct taskmanager *tm = JSObjectGetPrivate(object);

	JSValueRef exception = NULL;
	JSValueRef args[2];

	if (tm->callback == NULL)
		return;

	args[0] = JSValueMakeNumber(tm->context, pid);
	args[1] = JSValueMakeNumber(tm->context, status);

	(void)JSObjectCallAsFunction(tm->context, tm->callback,
			object, G_N_ELEMENTS(args), args, &exception);
	if (exception)
		g_warning(JS_LOG_CALLBACK_EXCEPTION, __func__);
}

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
			JS_ERR_INVALID_OBJECT_TEXT);
		return JSValueMakeNumber(context, -EINVAL);
	}

	if (argc != 1) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_ARG_COUNT);
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

	ret = task_manager_exec(tm->rcd->rc, command, taskmanager_terminate_cb,
			object);
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
			JS_ERR_INVALID_OBJECT_TEXT);
		return JSValueMakeNumber(context, -EINVAL);
	}

	if (argc != 2) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_ARG_COUNT);
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
	if (!tm) {
		g_warning("js-taskmanager: failed to allocate memory");
		return NULL;
	}

	tm->rcd = data->rcd;
	tm->context = context;

	return tm;
}

static void taskmanager_finalize(JSObjectRef object)
{
	struct taskmanager *tm = JSObjectGetPrivate(object);

	if (tm->callback)
		JSValueUnprotect(tm->context, tm->callback);

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
	.staticValues = taskmanager_properties,
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
