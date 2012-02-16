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
	GdkWindow *window;

	JSContextRef context;
	JSObjectRef callback;
	JSObjectRef thisptr;
};

static int cursor_move_to(int x, int y)
{
	GdkDisplay *display = NULL;
	GdkScreen *screen = NULL;

	display = gdk_display_get_default();
	if (!display)
		return FALSE;
	screen = gdk_display_get_default_screen(display);
	if (!screen)
		return FALSE;

	/* get cursor position, maybe we need this later */
	/*gdk_display_get_pointer(display, NULL, &x, &y, NULL);*/

	/* set the new cursor pos */
	gdk_display_warp_pointer(display, screen, x, y);
	gdk_display_flush(display);

	return TRUE;
}

static int cursor_enable(GdkWindow *window, gboolean enable)
{
	GdkCursor *cursor;

	if (!window)
		return FALSE;

	if (enable) {
		cursor = gdk_window_get_cursor(gdk_get_default_root_window());
		if (!cursor)
			cursor = gdk_cursor_new(GDK_X_CURSOR);
	}
	else
		cursor = gdk_cursor_new(GDK_BLANK_CURSOR);

	if (!cursor)
		return FALSE;

	gdk_window_set_cursor(window, cursor);
	gdk_cursor_unref(cursor);
	return TRUE;
}

static int cursor_click(GdkWindow *window, int x, int y)
{
	if (!window)
		return FALSE;

	gdk_test_simulate_button(window, x, y, 1, GDK_BUTTON1_MASK, GDK_BUTTON_PRESS);
	gdk_test_simulate_button(window, x, y, 1, GDK_BUTTON1_MASK, GDK_BUTTON_RELEASE);
	return TRUE;
}

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
                                         JSObjectRef thisObject,
                                         size_t argumentCount,
                                         const JSValueRef arguments[],
                                         JSValueRef *exception)
{
	int x, y;

	if (argumentCount != 2) {
		set_exception_text(context, exception,
			"invalid argument count");
		goto invalid_arg;
	}

	if (!JSValueIsNumber(context, arguments[0])) {
		set_exception_text(context, exception,
			"first argument is not a number");
		goto invalid_arg;
	}
	if (!JSValueIsNumber(context, arguments[1])) {
		set_exception_text(context, exception,
			"second argument is not a number");
		goto invalid_arg;
	}

	x = JSValueToNumber(context, arguments[0], exception);
	y = JSValueToNumber(context, arguments[1], exception);

	return JSValueMakeBoolean(context, cursor_move_to(x, y));

invalid_arg:
	return JSValueMakeBoolean(context, FALSE);
}

static JSValueRef cursor_clickat_callback(JSContextRef context,
                                          JSObjectRef function,
                                          JSObjectRef thisObject,
                                          size_t argumentCount,
                                          const JSValueRef arguments[],
                                          JSValueRef *exception)
{
	struct cursor *cursor = JSObjectGetPrivate(thisObject);
	GdkWindow *window = cursor->window;
	int x, y;

	if (argumentCount != 2) {
		set_exception_text(context, exception,
			"invalid argument count");
		goto invalid_arg;
	}

	if (!JSValueIsNumber(context, arguments[0])) {
		set_exception_text(context, exception,
			"first argument is not a number");
		goto invalid_arg;
	}
	if (!JSValueIsNumber(context, arguments[1])) {
		set_exception_text(context, exception,
			"second argument is not a number");
		goto invalid_arg;
	}

	x = JSValueToNumber(context, arguments[0], exception);
	y = JSValueToNumber(context, arguments[1], exception);

	return JSValueMakeBoolean(context, cursor_click(window, x, y));

invalid_arg:
	return JSValueMakeBoolean(context, FALSE);
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
                                  JSStringRef propertyName,
                                  JSValueRef *exception)
{
	/* FIXME: add query support */
	set_exception_text(context, exception, "write only property");
	return JSValueMakeBoolean(context, FALSE);
}

static bool cursor_set_show(JSContextRef context, JSObjectRef object,
                            JSStringRef propertyName, JSValueRef value,
                            JSValueRef *exception)
{
	struct cursor *cursor = JSObjectGetPrivate(object);
	GdkWindow *window = cursor->window;

	if (!JSValueIsBoolean(context, value)) {
		set_exception_text(context, exception, "not a boolean value");
		return false;
	}

	if (!cursor_enable(window, JSValueToBoolean(context, value))) {
		set_exception_text(context, exception, "set failed");
		return false;
	}

	return true;
}

static const JSStaticValue cursor_properties[] = {
	{
		.name = "show",
		.getProperty = cursor_get_show /*NULL ?*/,
		.setProperty = cursor_set_show,
		.attributes = kJSPropertyAttributeNone,
	}, {
	}
};

static void cursor_initialize(JSContextRef context, JSObjectRef object)
{
	struct cursor *cursor = JSObjectGetPrivate(object);
	cursor->thisptr = object;
}

static void cursor_finalize(JSObjectRef object)
{
	/* TODO: add cleanup code.... */
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
	JSObjectRef object;
	JSStringRef string;
	struct cursor *ctx;
	GtkWidget *widget;

	ctx = g_new0(struct cursor, 1);
	if (!ctx)
		return -ENOMEM;

	widget = GTK_WIDGET(webkit_web_frame_get_web_view(frame));
	g_assert(widget != NULL);
	ctx->window = gtk_widget_get_window(widget);
	g_assert(ctx->window != NULL);

	object = JSObjectMake(js, cursor_class, ctx);

	string = JSStringCreateWithUTF8CString(name);
	JSObjectSetProperty(js, parent, string, object, 0, &exception);
	JSStringRelease(string);

	return 0;
}
