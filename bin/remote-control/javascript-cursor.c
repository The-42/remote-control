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

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <gtk/gtk.h>

#include "javascript.h"

struct cursor {
	GdkDisplay *display;
	GdkScreen *screen;
	GdkWindow *window;
};

static void set_exception_text(JSContextRef context,JSValueRef *exception,
                               const char *failure)
{
	if (exception) {
		JSStringRef text = JSStringCreateWithUTF8CString(failure);
		*exception = JSValueMakeString(context, text);
		JSStringRelease(text);
	}
}

static JSValueRef cursor_moveto_callback(JSContextRef context,
                                         JSObjectRef function,
                                         JSObjectRef object,
                                         size_t argc, const JSValueRef argv[],
                                         JSValueRef *exception)
{
	struct cursor *priv = JSObjectGetPrivate(object);
	int x, y;

	if (!priv) {
		set_exception_text(context, exception,
			"object not valid, context switched?");
		return JSValueMakeBoolean(context, FALSE);
	}

	if (argc != 2) {
		set_exception_text(context, exception,
			"invalid argument count");
		return JSValueMakeBoolean(context, FALSE);
	}

	if (!JSValueIsNumber(context, argv[0])) {
		set_exception_text(context, exception,
			"x is not a number");
		return JSValueMakeBoolean(context, FALSE);
	}
	if (!JSValueIsNumber(context, argv[1])) {
		set_exception_text(context, exception,
			"y is not a number");
		return JSValueMakeBoolean(context, FALSE);
	}

	x = JSValueToNumber(context, argv[0], exception);
	y = JSValueToNumber(context, argv[1], exception);

	g_assert(priv->display != NULL);
	g_assert(priv->screen != NULL);

	gdk_display_warp_pointer(priv->display, priv->screen, x, y);
	gdk_display_flush(priv->display);

	return JSValueMakeBoolean(context, TRUE);
}

static JSValueRef cursor_clickat_callback(JSContextRef context,
                                          JSObjectRef function,
                                          JSObjectRef object,
                                          size_t argc, const JSValueRef argv[],
                                          JSValueRef *exception)
{
	struct cursor *priv = JSObjectGetPrivate(object);
	int x, y;

	if (!priv) {
		set_exception_text(context, exception,
			"object not valid, context switched?");
		return JSValueMakeBoolean(context, FALSE);
	}

	if (argc != 2) {
		set_exception_text(context, exception,
			"invalid argument count");
		return JSValueMakeBoolean(context, FALSE);
	}

	if (!JSValueIsNumber(context, argv[0])) {
		set_exception_text(context, exception,
			"x is not a number");
		return JSValueMakeBoolean(context, FALSE);
	}
	if (!JSValueIsNumber(context, argv[1])) {
		set_exception_text(context, exception,
			"y is not a number");
		return JSValueMakeBoolean(context, FALSE);
	}

	x = JSValueToNumber(context, argv[0], exception);
	y = JSValueToNumber(context, argv[1], exception);

	g_assert(priv->window != NULL);

	gdk_test_simulate_button(priv->window, x, y, 1, GDK_BUTTON1_MASK,
	                         GDK_BUTTON_PRESS);
	gdk_test_simulate_button(priv->window, x, y, 1, GDK_BUTTON1_MASK,
	                         GDK_BUTTON_RELEASE);

	return JSValueMakeBoolean(context, TRUE);
}

static const JSStaticFunction cursor_functions[] = {
	{
		.name = "moveTo",
		.callAsFunction = cursor_moveto_callback,
		.attributes = kJSPropertyAttributeNone,
	}, {
		.name = "clickAt",
		.callAsFunction = cursor_clickat_callback,
		.attributes = kJSPropertyAttributeNone,
	}, {
	}
};

static JSValueRef cursor_get_show(JSContextRef context, JSObjectRef object,
                                  JSStringRef property, JSValueRef *exception)
{
	struct cursor *priv = JSObjectGetPrivate(object);
	GdkCursor *cursor;

	if (!priv) {
		set_exception_text(context, exception,
			"object not valid, context switched?");
		return JSValueMakeBoolean(context, FALSE);
	}

	g_assert(priv->window != NULL);

	cursor = gdk_window_get_cursor(priv->window);
	if (!cursor) {
		set_exception_text(context, exception, "window has no cursor");
		return JSValueMakeBoolean(context, FALSE);
	}

	if (gdk_cursor_get_cursor_type(cursor) == GDK_BLANK_CURSOR)
		return JSValueMakeBoolean(context, FALSE);

	return JSValueMakeBoolean(context, TRUE);
}

static bool cursor_set_show(JSContextRef context, JSObjectRef object,
                            JSStringRef property, JSValueRef value,
                            JSValueRef *exception)
{
	struct cursor *priv = JSObjectGetPrivate(object);
	GdkCursor *cursor;

	if (!priv) {
		set_exception_text(context, exception,
			"object not valid, context switched?");
		return false;
	}


	if (!JSValueIsBoolean(context, value)) {
		set_exception_text(context, exception, "not a boolean");
		return false;
	}

	if (JSValueToBoolean(context, value)) {
		cursor = gdk_window_get_cursor(priv->window);
		if (!cursor) {
			cursor = gdk_cursor_new_for_display(priv->display,
			                                    GDK_X_CURSOR);
		}
	} else {
		cursor = gdk_cursor_new_for_display(priv->display,
		                                    GDK_BLANK_CURSOR);
	}

	g_assert(priv->window != NULL);

	gdk_window_set_cursor(priv->window, cursor);
	gdk_cursor_unref(cursor);

	return true;
}

static const JSStaticValue cursor_properties[] = {
	{
		.name = "show",
		.getProperty = cursor_get_show,
		.setProperty = cursor_set_show,
		.attributes = kJSPropertyAttributeNone,
	}, {
	}
};

static void cursor_initialize(JSContextRef context, JSObjectRef object)
{
}

static void cursor_finalize(JSObjectRef object)
{
	struct cursor *priv = JSObjectGetPrivate(object);

	g_free(priv);
	JSObjectSetPrivate(object, NULL);
}

static const JSClassDefinition cursor_classdef = {
	.className = "Cursor",
	.initialize = cursor_initialize,
	.finalize = cursor_finalize,
	.staticValues = cursor_properties,
	.staticFunctions = cursor_functions,
};

static JSClassRef cursor_class = NULL;

int javascript_register_cursor_class(void)
{
	cursor_class = JSClassCreate(&cursor_classdef);
	if (!cursor_class) {
		g_warning("%s: failed to create Cursor class", __func__);
		return -ENOMEM;
	}

	return 0;
}

int javascript_register_cursor(JSContextRef js, WebKitWebFrame *frame,
                               JSObjectRef parent, const char *name)
{
	JSValueRef exception = NULL;
	struct cursor *priv;
	WebKitWebView *view;
	JSObjectRef object;
	JSStringRef string;

	priv = g_new0(struct cursor, 1);
	if (!priv)
		return -ENOMEM;

	view = webkit_web_frame_get_web_view(frame);
	g_assert(view != NULL);

	priv->display = gdk_display_get_default();
	g_assert(priv->display != NULL);

	priv->screen = gdk_display_get_default_screen(priv->display);
	g_assert(priv->screen != NULL);

	priv->window = gtk_widget_get_window(GTK_WIDGET(view));
	g_assert(priv->window != NULL);

	object = JSObjectMake(js, cursor_class, priv);

	string = JSStringCreateWithUTF8CString(name);
	JSObjectSetProperty(js, parent, string, object, 0, &exception);
	JSStringRelease(string);

	return 0;
}
