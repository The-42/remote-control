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

#include "javascript.h"

extern struct javascript_module javascript_cursor;
extern struct javascript_module javascript_input;
extern struct javascript_module javascript_cursor;
extern struct javascript_module javascript_lcd;
extern struct javascript_module javascript_app_watchdog;
extern struct javascript_module javascript_monitor;
extern struct javascript_module javascript_taskmanager;

static struct javascript_module *ad_modules[] = {
	&javascript_cursor,
	&javascript_input,
#ifdef ENABLE_JAVASCRIPT_IR
	&javascript_cursor,
#endif
#ifdef ENABLE_JAVASCRIPT_LCD
	&javascript_lcd,
#endif
#ifdef ENABLE_JAVASCRIPT_APP_WATCHDOG
	&javascript_app_watchdog,
#endif
	&javascript_monitor,
	&javascript_taskmanager,
	NULL
};

void javascript_set_exception_text(JSContextRef context,JSValueRef *exception,
                               const char *failure)
{
	if (exception) {
		JSStringRef text = JSStringCreateWithUTF8CString(failure);
		*exception = JSValueMakeString(context, text);
		JSStringRelease(text);
	}
}

static int javascript_register_module(JSGlobalContextRef js,
				JSObjectRef parent,
				struct javascript_module *module,
				struct javascript_userdata *data)
{
	JSObjectRef object;
	JSStringRef string;

	string = JSStringCreateWithUTF8CString(module->classdef->className);
	if (!string)
		return -ENOMEM;

	object = module->create(js, module->class, data);
	if (object)
		JSObjectSetProperty(js, parent, string, object, 0, NULL);

	JSStringRelease(string);
	return object ? 0 : -ENODEV;
}

static int javascript_register_avionic_design(JSGlobalContextRef js,
                                              JSObjectRef parent,
                                              const char *name,
					      struct javascript_userdata *data)
{
	JSValueRef exception = NULL;
	JSObjectRef object;
	JSStringRef string;
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

	string = JSStringCreateWithUTF8CString(name);
	if (string) {
		JSObjectSetProperty(js, parent, string, object, 0, &exception);
		JSStringRelease(string);
	}

	return 0;
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
	if (err < 0) {
		g_debug("failed to register AvionicDesign object: %s",
				g_strerror(-err));
		return err;
	}

	return 0;
}
