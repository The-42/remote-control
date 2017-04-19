/*
 * Copyright (C) 2012 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <JavaScriptCore/JavaScript.h>
#include <glib.h>
#include <errno.h>
#include <math.h>
#include <string.h>

#include "javascript.h"

extern struct javascript_module javascript_cursor;
extern struct javascript_module javascript_input;
extern struct javascript_module javascript_ir;
extern struct javascript_module javascript_lcd;
extern struct javascript_module javascript_app_watchdog;
extern struct javascript_module javascript_taskmanager;
extern struct javascript_module javascript_audio;
extern struct javascript_module javascript_audio_player;
extern struct javascript_module javascript_backlight;
extern struct javascript_module javascript_media_player;
extern struct javascript_module javascript_medial;
extern struct javascript_module javascript_modem;
extern struct javascript_module javascript_voip;
extern struct javascript_module javascript_output;
extern struct javascript_module javascript_smartcard;
extern struct javascript_module javascript_fb;
extern struct javascript_module javascript_http_request;
extern struct javascript_module javascript_sysinfo;
extern struct javascript_module javascript_lldp;
extern struct javascript_module javascript_event_manager;

static struct javascript_module *ad_modules[] = {
	&javascript_cursor,
	&javascript_input,
#ifdef ENABLE_JAVASCRIPT_IR
	&javascript_ir,
#endif
#ifdef ENABLE_JAVASCRIPT_LCD
	&javascript_lcd,
#endif
#ifdef ENABLE_JAVASCRIPT_APP_WATCHDOG
	&javascript_app_watchdog,
#endif
	&javascript_taskmanager,
	&javascript_audio,
	&javascript_audio_player,
	&javascript_backlight,
	&javascript_media_player,
#ifdef ENABLE_JAVASCRIPT_MEDIAL
	&javascript_medial,
#endif
#ifdef ENABLE_LIBMODEM
	&javascript_modem,
#endif
	&javascript_voip,
	&javascript_output,
	&javascript_smartcard,
	&javascript_fb,
	&javascript_http_request,
	&javascript_sysinfo,
	&javascript_lldp,
	&javascript_event_manager,
	NULL
};

JSValueRef javascript_make_string(
	JSContextRef context, const char *cstr, JSValueRef *exception)
{
	JSValueRef value;
	JSStringRef str;

	if (cstr == NULL)
		return NULL;

	str = JSStringCreateWithUTF8CString(cstr);
	if (!str) {
		javascript_set_exception_text(context, exception,
			"failed to create string");
		return NULL;
	}

	value = JSValueMakeString(context, str);
	JSStringRelease(str);

	return value;
}

JSValueRef javascript_vsprintf(
	JSContextRef context, JSValueRef *exception,
	const char *cstr, va_list ap)
{
	JSValueRef val = NULL;
	char *buffer;
	va_list aq;
	int len;

	va_copy(aq, ap);
	len = vsnprintf(NULL, 0, cstr, aq);
	va_end(aq);

	if (len < 0) {
		javascript_set_exception_text(context, exception,
			"failed to get string buffer size");
		return NULL;
	} else
		len += 1; /* count the terinator */

	buffer = g_malloc(len);
	if (!buffer) {
		javascript_set_exception_text(context, exception,
			"failed to get allocate buffer");
		return NULL;
	}
	if (vsnprintf(buffer, len, cstr, ap) < 0)
		javascript_set_exception_text(context, exception,
			"failed to format string");
	else
		val = javascript_make_string(context, buffer, exception);
	g_free(buffer);

	return val;
}

JSValueRef javascript_sprintf(
	JSContextRef context, JSValueRef *exception,
	const char *cstr, ...)
{
	JSValueRef val;
	va_list ap;

	va_start(ap, cstr);
	val = javascript_vsprintf(context, exception, cstr, ap);
	va_end(ap);

	return val;
}

void javascript_printf_exception_text(JSContextRef context,
	JSValueRef *exception, const char *failure, ...)
{
	if (exception) {
		va_list ap;
		va_start(ap, failure);
		/* We don't pass the exception here to avoid a loop if
		 * javascript_vsprintf() fails. */
		*exception = javascript_vsprintf(context, NULL, failure, ap);
		va_end(ap);
	}
}

char* javascript_get_string(JSContextRef context, const JSValueRef val,
			JSValueRef *exception)
{
	JSStringRef str;
	size_t size;
	char *buffer;

	str = JSValueToStringCopy(context, val, exception);
	if (!str)
		return NULL;

	size = JSStringGetMaximumUTF8CStringSize(str);
	buffer = g_malloc(size);

	if (buffer)
		JSStringGetUTF8CString(str, buffer, size);
	else
		javascript_set_exception_text(context, exception,
			"failed to allocated string buffer");

	JSStringRelease(str);
	return buffer;
}

int javascript_int_from_number(JSContextRef context, const JSValueRef val,
		int min, int max, int *ret, JSValueRef *exception)
{
	double dval;

	dval = JSValueToNumber(context, val, exception);
	if (isnan(dval))
		return -EINVAL;

	if (!ret)
		return 0;

	if (dval < min)
		*ret = min;
	else if (dval > max)
		*ret = max;
	else
		*ret = dval;

	return 0;
}

int javascript_int_from_unit(JSContextRef context, const JSValueRef val,
		int min, int max, int *ret, JSValueRef *exception)
{
	double dval;

	dval = JSValueToNumber(context, val, exception);
	if (isnan(dval))
		return -EINVAL;

	if (!ret)
		return 0;

	dval = dval * (max - min) + min;
	if (dval < min)
		*ret = min;
	else if (dval > max)
		*ret = max;
	else
		*ret = dval;

	return 0;
}

JSValueRef javascript_int_to_unit(JSContextRef context, int val,
		int min, int max)
{
	double dval = (double)(val - min) / (max - min);
	return JSValueMakeNumber(context, dval);
}

int javascript_enum_from_string(
	JSContextRef context, const struct javascript_enum *desc,
	JSValueRef value, int *ret, JSValueRef *exception)
{
	JSStringRef str;
	int i;

	str = JSValueToStringCopy(context, value, exception);
	if (!str)
		return -ENOMEM;

	for (i = 0; desc[i].name; i++) {
		if (JSStringIsEqualToUTF8CString(str, desc[i].name))
			break;
	}

	JSStringRelease(str);

	if (!desc[i].name) {
		javascript_set_exception_text(context, exception,
					"unknown enum value");
		return -EINVAL;
	}

	if (ret)
		*ret = desc[i].value;

	return 0;
}

JSValueRef javascript_enum_to_string(
	JSContextRef context, const struct javascript_enum *desc,
	int val, JSValueRef *exception)
{
	JSValueRef value;
	JSStringRef str;
	int i;

	for (i = 0; desc[i].name; i++) {
		if (desc[i].value == val)
			break;
	}

	if (!desc[i].name) {
		javascript_set_exception_text(context, exception,
					"got unknown enum value");
		return NULL;
	}

	str = JSStringCreateWithUTF8CString(desc[i].name);
	if (!str) {
		javascript_set_exception_text(context, exception,
					"failed to create string");
		return NULL;
	}

	value = JSValueMakeString(context, str);
	JSStringRelease(str);
	return value;
}

JSValueRef javascript_object_get_property(
	JSContextRef context, JSObjectRef object,
	const char *prop, JSValueRef *exception)
{
	JSValueRef value;
	JSStringRef str;

	str = JSStringCreateWithUTF8CString(prop);
	if (!str) {
		javascript_set_exception_text(context, exception,
			"failed to create property name string");
		return NULL;
	}

	value = JSObjectGetProperty(context, object, str, exception);
	JSStringRelease(str);
	return value;
}

int javascript_object_set_property(
	JSContextRef context, JSObjectRef object,
	const char *prop, JSValueRef value,
	JSPropertyAttributes attr,
	JSValueRef *exception)
{
	JSValueRef excp = NULL;
	JSStringRef str;

	char *path_pos = strchr(prop, '.');
	if (path_pos) {
		char *path_name = g_strndup(prop, path_pos - prop);
		JSStringRef path_str;
		JSObjectRef path_obj;

		if (!path_name) {
			javascript_set_exception_text(context, exception,
					"failed to create path name");
			return -ENOMEM;
		}
		path_str = JSStringCreateWithUTF8CString(path_name);
		g_free(path_name);
		if (!path_str) {
			javascript_set_exception_text(context, exception,
					"failed to create path string");
			return -ENOMEM;
		}
		if (JSObjectHasProperty(context, object, path_str)) {
			path_obj = (JSObjectRef)JSObjectGetProperty(context,
					object, path_str, exception);
		} else {
			path_obj = JSObjectMake(context, NULL, NULL);
			if (!path_obj) {
				JSStringRelease(path_str);
				javascript_set_exception_text(context, exception,
						"failed to create path object");
				return -ENOMEM;
			}
			JSObjectSetProperty(context, object, path_str, path_obj,
					0, NULL);
		}
		JSStringRelease(path_str);

		return javascript_object_set_property(context, path_obj,
				&path_pos[1], value, attr, exception);
	}

	str = JSStringCreateWithUTF8CString(prop);
	if (!str) {
		javascript_set_exception_text(context, exception,
			"failed to create property name string");
		return -ENOMEM;
	}

	if (!exception)
		exception = &excp;

	JSObjectSetProperty(context, object, str, value, attr, exception);
	JSStringRelease(str);

	return *exception == NULL ? 0 : -EINVAL;
}

int javascript_buffer_from_object(
	JSContextRef context, JSObjectRef array,
	char **bufferp, JSValueRef *exception)
{
	JSPropertyNameArrayRef props;
	char *buffer;
	int i, size;

	props = JSObjectCopyPropertyNames(context, array);
	if (!props) {
		javascript_set_exception_text(context, exception,
					"failed to get property names");
		return -ENOMEM;
	}

	size = JSPropertyNameArrayGetCount(props);
	if (size == 0) {
		if (bufferp)
			*bufferp = NULL;
		JSPropertyNameArrayRelease(props);
		return 0;
	}

	buffer = g_malloc(size);
	if (!buffer) {
		javascript_set_exception_text(context, exception,
					"failed to allocate buffer");
		JSPropertyNameArrayRelease(props);
		return -ENOMEM;
	}

	for (i = 0; i < size; i++) {
		JSStringRef name =
			JSPropertyNameArrayGetNameAtIndex(props, i);
		JSValueRef value =
			JSObjectGetProperty(context, array, name, NULL);
		double dval = JSValueToNumber(context, value, NULL);
		if (isnan(dval)) {
			javascript_set_exception_text(context, exception,
						"value isn't a number");
			g_free(buffer);
			JSPropertyNameArrayRelease(props);
			return -EINVAL;
		}
		buffer[i] = dval;
	}

	if (bufferp)
		*bufferp = buffer;
	else
		g_free(buffer);

	return size;
}

int javascript_buffer_from_value(
	JSContextRef context, JSValueRef array,
	char **bufferp, JSValueRef *exception)
{
	JSObjectRef obj;

	obj = JSValueToObject(context, array, exception);
	if (!obj)
		return -EINVAL;

	return javascript_buffer_from_object(
		context, obj, bufferp, exception);
}

JSObjectRef javascript_buffer_to_object(
	JSContextRef context, char *buffer, size_t size,
	JSValueRef *exception)
{
	JSValueRef *values = NULL;
	JSObjectRef array;
	int i;

	if (size > 0) {
		values = g_malloc0(size * sizeof(*values));
		if (!values) {
			javascript_set_exception_text(context, exception,
						"failed to allocate values buffer");
			return NULL;
		}

		for (i = 0; i < size; i++)
			values[i] = JSValueMakeNumber(context, buffer[i]);
	}

	array = JSObjectMakeArray(context, size, values, exception);
	g_free(values);

	return array;
}

char *javascript_config_get_string(GKeyFile *config, const char *group,
		const char *name, const char *key)
{
	char *group_name;
	char *value;

	group_name = g_strdup_printf("%s%s", group, name);
	if (!group_name)
		return NULL;

	value = g_key_file_get_string(config, group_name, key, NULL);
	g_free(group_name);

	return value;
}

char **javascript_config_get_string_list(GKeyFile *config, const char *group,
		const char *name, const char *key)
{
	char *group_name;
	char **value;

	group_name = g_strdup_printf("%s%s", group, name);
	if (!group_name)
		return NULL;

	value = g_key_file_get_string_list(
		config, group_name, key, NULL, NULL);
	g_free(group_name);

	return value;
}

double javascript_config_get_double(GKeyFile *config, const char *group,
		const char *name, const char *key)
{
	GError *err = NULL;
	char *group_name;
	double value;

	group_name = g_strdup_printf("%s%s", group, name);
	if (!group_name)
		return NAN;

	value = g_key_file_get_double(config, group_name, key, &err);
	g_free(group_name);
	if (err)
		g_error_free(err);

	return err ? NAN : value;
}

gint javascript_config_get_integer(GKeyFile *config, const char *group,
		const char *name, const char *key)
{
	GError *err = NULL;
	char *group_name;
	gint value;

	group_name = g_strdup_printf("%s%s", group, name);
	if (!group_name)
		return 0;

	value = g_key_file_get_integer(config, group_name, key, &err);
	g_free(group_name);
	if (err)
		g_error_free(err);

	return err ? 0 : value;
}


gchar **javascript_config_get_groups(GKeyFile *config, const char *group)
{
	gchar **ret = NULL;
	gsize num_groups;
	gchar **names;
	int i, j = 0;

	names = g_key_file_get_groups(config, &num_groups);
	if (!names)
		goto cleanup;

	ret = g_new (gchar *, num_groups + 1);
	if (!ret)
		goto cleanup;


	for (i = 0; names[i]; i++) {
		if (!g_str_has_prefix(names[i], group))
			continue;
		ret[j++] = g_strdup(names[i] + strlen(group));
	}
	ret[j] = NULL;

cleanup:
	g_strfreev(names);
	return ret;
}

static int javascript_register_module(JSGlobalContextRef js,
				JSObjectRef parent,
				struct javascript_module *module,
				struct javascript_userdata *data)
{
	JSObjectRef object;

	if (!module->create)
		return 0;

	object = module->create(js, module->class, data);
	if (!object)
		return -ENODEV;

	return javascript_object_set_property(
		js, parent, module->classdef->className, object, 0, NULL);
}

static int javascript_register_avionic_design(JSGlobalContextRef js,
                                              JSObjectRef parent,
                                              const char *name,
					      struct javascript_userdata *data)
{
	JSValueRef api_version;
	JSObjectRef object;
	int i, err;

	object = JSObjectMake(js, NULL, NULL);

	for (i = 0; ad_modules[i]; i++) {
		err = javascript_register_module(
			js, object, ad_modules[i], data);
		if (err) {
			g_warning("%s: failed to register %s object: %s",
				__func__, ad_modules[i]->classdef->className,
				g_strerror(-err));
			if (err != -ENODEV)
				return err;
		}
	}

	api_version = JSValueMakeNumber(js, JS_API_VERSION);
	err = javascript_object_set_property(js, object, "version", api_version,
		kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly,
		NULL);

	if (err) {
		g_warning("%s: failed to set version property on %s object",
			__func__, name);
	}

	return javascript_object_set_property(
		js, parent, name, object, 0, NULL);
}

static int javascript_register_classes(void)
{
	int i;

	for (i = 0; ad_modules[i]; i++) {
		ad_modules[i]->class = JSClassCreate(ad_modules[i]->classdef);
		if (!ad_modules[i]->class) {
			g_warning("%s: failed to register %s class",
				__func__, ad_modules[i]->classdef->className);
			return -ENOMEM;
		}
	}

	return 0;
}

static void javascript_release_classes(void)
{
	int i;

	for (i = 0; ad_modules[i]; i++) {
		if (ad_modules[i]->class) {
			JSClassRelease(ad_modules[i]->class);
			ad_modules[i]->class = NULL;
		}
	}
}

int javascript_register(JSGlobalContextRef context,
                        struct javascript_userdata *user_data)
{
	JSObjectRef object;
	int err;

	err = javascript_register_classes();
	if (err < 0) {
		g_debug("failed to register JavaScript classes: %s",
				g_strerror(-err));
		return err;
	}

	object = JSContextGetGlobalObject(context);

	err = javascript_register_avionic_design(context, object,
	                                         "AvionicDesign",
	                                         user_data);
	javascript_release_classes();

	if (err < 0) {
		g_debug("failed to register AvionicDesign object: %s",
				g_strerror(-err));
		return err;
	}

	return 0;
}

int javascript_init(GKeyFile *config)
{
	int i, err;

	for (i = 0; ad_modules[i]; i++) {
		if (!ad_modules[i]->init)
			continue;
		err = ad_modules[i]->init(config);
		if (err) {
			g_debug("failed to init JS module %s: %s",
				ad_modules[i]->classdef->className,
				g_strerror(-err));
			return err;
		}
	}

	return 0;
}
