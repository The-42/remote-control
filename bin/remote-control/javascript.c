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

#include "javascript.h"

static int javascript_register_avionic_design(JSGlobalContextRef js,
                                              JSObjectRef parent,
                                              const char *name,
                                              GMainLoop *loop,
                                              RemoteControlWebkitWindow *window)
{
	JSValueRef exception = NULL;
	JSObjectRef object;
	JSStringRef string;
	int err;

	object = JSObjectMake(js, NULL, NULL);

	err = javascript_register_cursor(js, object, "Cursor", window);
	if (err < 0) {
		g_warning("%s: failed to register Cursor object: %s",
			__func__, g_strerror(-err));
		return err;
	}

	err = javascript_register_input(js, object, "Input", loop);
	if (err < 0) {
		g_warning("%s: failed to register Input object: %s",
			__func__, g_strerror(-err));
		return err;
	}

	err = javascript_register_ir(js, object, "IR", loop);
	if (err < 0) {
		g_warning("%s: failed to register IR object: %s",
			__func__, g_strerror(-err));
		return err;
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
	int err;

	err = javascript_register_cursor_class();
	if (err < 0) {
		g_warning("%s: failed to register cursor class: %s",
			__func__, g_strerror(-err));
		return err;
	}

	err = javascript_register_input_class();
	if (err < 0) {
		g_warning("%s: failed to register cursor class: %s",
			__func__, g_strerror(-err));
		return err;
	}

	err = javascript_register_ir_class();
	if (err < 0) {
		g_warning("%s: failed to register cursor class: %s",
			__func__, g_strerror(-err));
		return err;
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
	                                         user_data->loop,
	                                         user_data->window);
	if (err < 0) {
		g_debug("failed to register AvionicDesign object: %s",
				g_strerror(-err));
		return err;
	}

	return 0;
}
