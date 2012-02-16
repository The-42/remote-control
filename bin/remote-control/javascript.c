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
		GMainContext *context, WebKitWebFrame *frame,
		JSObjectRef parent, const char *name)
{
	JSValueRef exception = NULL;
	JSObjectRef object;
	JSStringRef string;
	int errors = 0;
	int err;

	object = JSObjectMake(js, NULL, NULL);

	err = javascript_register_cursor(js, frame, object, "Cursor");
	if (err < 0) {
		g_warning("%s: failed to register Cursor object: %s",
			__func__, g_strerror(-err));
		errors++;
	}

	err = javascript_register_input(js, context, object, "Input");
	if (err < 0) {
		g_warning("%s: failed to register Input object: %s",
			__func__, g_strerror(-err));
		errors++;
	}

	string = JSStringCreateWithUTF8CString(name);
	if (string) {
		JSObjectSetProperty(js, parent, string, object, 0, &exception);
		JSStringRelease(string);
	}

	return errors == 0;
}

static int javascript_register_classes(void)
{
	int errors = 0;
	int err;

	err = javascript_register_cursor_class();
	if (err < 0) {
		g_warning("%s: failed to register cursor class: %s",
			__func__, g_strerror(-err));
		errors++;
	}
	err = javascript_register_input_class();
	if (err < 0) {
		g_warning("%s: failed to register cursor class: %s",
			__func__, g_strerror(-err));
		errors++;
	}

	return errors == 0;
}

int javascript_register(WebKitWebFrame *frame, GMainContext *context)
{
	JSGlobalContextRef jsc;
	JSObjectRef object;
	int err;

	jsc = webkit_web_frame_get_global_context(frame);
	g_assert(jsc != NULL);

	err = javascript_register_classes();
	if (err < 0) {
		g_debug("failed to register JavaScript classes: %s",
				g_strerror(-err));
		return err;
	}

	object = JSContextGetGlobalObject(jsc);

	err = javascript_register_avionic_design(jsc, context, frame, object,
			"AvionicDesign");
	if (err < 0) {
		g_debug("failed to register AvionicDesign object: %s",
				g_strerror(-err));
		return err;
	}

	return 0;
}
