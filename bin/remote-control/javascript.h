/*
 * Copyright (C) 2012 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef JAVASCRIPT_API_H
#define JAVASCRIPT_API_H 1

#include <JavaScriptCore/JavaScript.h>
#include "remote-control-webkit-window.h"
#include "remote-control-data.h"

struct javascript_userdata {
	RemoteControlWebkitWindow *window;
	struct remote_control_data *rcd;
	GMainLoop *loop;
};

struct javascript_module {
	const JSClassDefinition	*classdef;
	JSObjectRef (*create)(JSContextRef js, JSClassRef class,
			struct javascript_userdata *data);

	JSClassRef		class;
};

struct javascript_enum {
	const char *name;
	int value;
};

void javascript_set_exception_text(JSContextRef context,JSValueRef *exception,
				const char *failure);

JSValueRef javascript_make_string(JSContextRef context, const char *cstr,
				JSValueRef *exception);

char* javascript_get_string(JSContextRef context, const JSValueRef val,
			JSValueRef *exception);

int javascript_enum_from_string(
	JSContextRef context, const struct javascript_enum *desc,
	JSValueRef value, int *ret, JSValueRef *exception);

JSValueRef javascript_enum_to_string(
	JSContextRef context, const struct javascript_enum *desc,
	int val, JSValueRef *exception);

int javascript_int_from_number(
	JSContextRef context, const JSValueRef val,
	int min, int max, int *ret, JSValueRef *exception);

int javascript_int_from_unit(
	JSContextRef context, const JSValueRef val,
	int min, int max, int *ret, JSValueRef *exception);

JSValueRef javascript_int_to_unit(
	JSContextRef context, int val, int min, int max);

int javascript_register(JSGlobalContextRef js,
			struct javascript_userdata *user_data);

#endif /* JAVASCRIPT_API_H */
