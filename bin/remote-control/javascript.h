/*
 * Copyright (C) 2012 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef JAVASCRIPT_API_H
#define JAVASCRIPT_API_H 1

#include <stdarg.h>
#include <JavaScriptCore/JavaScript.h>
#include "remote-control-webkit-window.h"
#include "remote-control-data.h"

/* JS API version returned from the window.AvionicDesign.version property */
#ifndef JS_API_VERSION
  #define JS_API_VERSION	1
#endif

#define JS_ERR_INVALID_OBJECT_TEXT      "object not valid, context switched?"
#define JS_ERR_INVALID_ARG_COUNT        "invalid argument count"
#define JS_ERR_INVALID_NUMBER           "argument is not a valid number"

#define JS_LOG_CALLBACK_EXCEPTION       "%s: Exception in callback. " \
                                        "Page unloaded without clearing the callback?"

struct javascript_userdata {
	RemoteControlWebkitWindow *window;
	struct remote_control_data *rcd;
	GMainLoop *loop;
};

struct javascript_module {
	const JSClassDefinition	*classdef;
	int (*init)(GKeyFile *config);
	JSObjectRef (*create)(JSContextRef js, JSClassRef class,
			struct javascript_userdata *data);

	JSClassRef		class;
};

struct javascript_enum {
	const char *name;
	int value;
};

void javascript_printf_exception_text(JSContextRef context,JSValueRef *exception,
				const char *failure, ...)
__attribute__((format(printf, 3, 4)));

#define javascript_set_exception_text(ctx, excp, msg, args...) \
	javascript_printf_exception_text(ctx, excp, "%s: " msg, __func__, ##args)

JSValueRef javascript_make_string(JSContextRef context, const char *cstr,
				JSValueRef *exception);

JSValueRef javascript_vsprintf(JSContextRef context, JSValueRef *exception,
			const char *cstr, va_list ap);

JSValueRef javascript_sprintf(JSContextRef context, JSValueRef *exception,
			const char *cstr, ...)
__attribute__((format(printf, 3, 4)));

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

JSValueRef javascript_object_get_property(
	JSContextRef context, JSObjectRef object,
	const char *prop, JSValueRef *exception);

int javascript_object_set_property(
	JSContextRef context, JSObjectRef object,
	const char *prop, JSValueRef value,
	JSPropertyAttributes attr,
	JSValueRef *exception);

int javascript_buffer_from_object(
	JSContextRef context, JSObjectRef array,
	char **bufferp, JSValueRef *exception);

int javascript_buffer_from_value(
	JSContextRef context, JSValueRef array,
	char **bufferp, JSValueRef *exception);

JSObjectRef javascript_buffer_to_object(
	JSContextRef context, char *buffer, size_t size,
	JSValueRef *exception);

char *javascript_config_get_string(GKeyFile *config, const char *group,
	const char *name, const char *key);

char **javascript_config_get_string_list(GKeyFile *config, const char *group,
	const char *name, const char *key);

double javascript_config_get_double(GKeyFile *config, const char *group,
	const char *name, const char *key);

gint javascript_config_get_integer(GKeyFile *config, const char *group,
	const char *name, const char *key);

gchar **javascript_config_get_groups(GKeyFile *config, const char *group);

int javascript_register(JSGlobalContextRef js,
			struct javascript_userdata *user_data);

int javascript_init(GKeyFile *config);

#endif /* JAVASCRIPT_API_H */
