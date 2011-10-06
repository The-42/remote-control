/*
 * Copyright (C) 2010-2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <JavaScriptCore/JavaScript.h>
#include <webkit/webkit.h>

#include "remote-control-webkit-jscript.h"

struct context {
	GdkWindow *window;
};

static struct context* avionic_get_context()
{
	static struct context *ctx = NULL;
	if (ctx == NULL) {
		ctx = malloc(sizeof(struct context));
		memset(ctx, 0, sizeof(*ctx));
	}
	return ctx;
}

static void avionic_set_user_context(gpointer data)
{
	struct context *ctx = avionic_get_context();
	if (ctx)
		ctx->window = GTK_WIDGET(data)->window;
}

static int avionic_get_user_context(JSContextRef context, struct context **ctx)
{
	if (!ctx)
		return -EINVAL;

	*ctx = avionic_get_context();
	return 0;
}

#if 0
static const char* printIndent(size_t indent)
{
	static const char * INDENT = "---------------------------------------+ ";
	static const size_t LENGTH = 42/*strlen(INDENT)*/;

	if (indent > LENGTH)
		indent = LENGTH;

	return &INDENT[LENGTH - indent];
}

static void printJSStringRef(JSStringRef string)
{
	size_t length;
	char *buffer;

	length = JSStringGetLength(string);
	buffer = malloc(length + 1);

	JSStringGetUTF8CString(string, buffer, length + 1);
	printf("%s", buffer);

	free(buffer);
}

static void printJSValueRef(JSContextRef ctx, JSValueRef argument, JSValueRef *exception, size_t indention);

static void printJSObjectRef(JSContextRef ctx, JSObjectRef argument, JSValueRef *exception, size_t indention)
{
	JSPropertyNameArrayRef names;
	const char* indent;
	size_t count;
	size_t i;

	names = JSObjectCopyPropertyNames(ctx, argument);
	count = JSPropertyNameArrayGetCount(names);
	indent = printIndent(indention);

	for (i = 0; i < count; i++) {
		JSStringRef name;

		name = JSPropertyNameArrayGetNameAtIndex(names, i);
		printf("%s[%2zu] ", indent, i);
		printJSStringRef(name);
		printf("\n");

		printJSValueRef(ctx, JSObjectGetPropertyAtIndex(ctx, argument, i, exception), exception, indention+5);
	}
}

static void printJSValueRef(JSContextRef ctx, JSValueRef argument, JSValueRef *exception, size_t indention)
{
	const char* indent;
	JSType type;

	indent = printIndent(indention);
	type = JSValueGetType(ctx, argument);
	switch(type) {
	case kJSTypeUndefined:
		printf("%skJSTypeUndefined\n", indent);
		break;
	case kJSTypeNull:
		printf("%skJSTypeNull\n", indent);
		break;
	case kJSTypeBoolean:
		printf("%skJSTypeBoolean\n", indent);
		printf("%s   %s\n", indent, JSValueToBoolean(ctx, argument) ? "True" : "False");
		break;
	case kJSTypeNumber:
		printf("%skJSTypeNumber\n", indent);
		printf("%s   %f\n", indent, JSValueToNumber(ctx, argument, exception));
		break;
	case kJSTypeString:
		printf("%skJSTypeString\n", indent);
		{
			JSStringRef s;
			s = JSValueToStringCopy(ctx, argument, exception);
			printf("%s", indent);
			printJSStringRef(s);
			JSStringRelease(s);
		}
		break;
	case kJSTypeObject:
		printf("%skJSTypeObject\n", indent);
		{
			JSObjectRef o;
			o = JSValueToObject(ctx, argument, exception);
			printJSObjectRef(ctx, o, exception, indention);
		}
		break;
	}
}

static JSValueRef avionic_event_register_callback_wrapper(JSContextRef context,
                                                          JSObjectRef func,
                                                          JSObjectRef self,
                                                          size_t argc,
                                                          const JSValueRef argv[],
                                                          JSValueRef *exception)
{
	size_t i;

	for (i = 0; i < argc; i++)
		printJSValueRef(context, argv[i], exception, 3);

	return JSValueMakeBoolean(context, true);
}
#endif

static gint avionic_cursor_set(struct context *ctx, gint x, gint y)
{
	GdkDisplay *display = NULL;
	GdkScreen *screen = NULL;

	g_debug("> %s(x=%d, y=%d)", __func__, x, y);

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

	g_debug("< %s()", __func__);
	return TRUE;
}

static JSValueRef avionic_cursor_set_wrapper(JSContextRef context,
                                             JSObjectRef func,
                                             JSObjectRef self,
                                             size_t argc,
                                             const JSValueRef argv[],
                                             JSValueRef *exception)
{
	struct context *user_context = NULL;
	bool ret;

	g_debug("> %s(context=%p, func=%p, self=%p, argc=%zu, argv=%p, exception=%p)",
		__func__, context, func, self, argc, argv, exception);

	if (avionic_get_user_context(context, &user_context) < 0 || !user_context) {
		g_warning("< %s() unable to get user context: false", __func__);
		return JSValueMakeBoolean(context, false);
	}

	/* check argument count, this function requires 1 parameter */
	if (argc != 2) {
		g_warning("< %s() invalid argument count %zu: false", __func__, argc);
		if (exception) {
			JSStringRef text = JSStringCreateWithUTF8CString("invalid argument count");
			*exception = JSValueMakeString(context, text);
			JSStringRelease(text);
		}
		return JSValueMakeBoolean(context, false);
	}
	/* check if type is as expected */
	if (!JSValueIsNumber(context, argv[0])) {
		g_warning("< %s() invalid argument[0] type %d: false",
			__func__, JSValueGetType(context, argv[0]));
		return JSValueMakeBoolean(context, false);
	}
	if (!JSValueIsNumber(context, argv[1])) {
		g_warning("< %s() invalid argument[1] type %d: false",
			__func__, JSValueGetType(context, argv[1]));
		return JSValueMakeBoolean(context, false);
	}
	/* call the function */
	ret = avionic_cursor_set(user_context,
	          (gint)JSValueToNumber(context, argv[0], exception),
	          (gint)JSValueToNumber(context, argv[1], exception));
	if (!ret) {
		g_warning("< %s() call failed: false", __func__);
		return JSValueMakeBoolean(context, false);
	}

	g_debug("< %s() = true", __func__);
	return JSValueMakeBoolean(context, true);
}

static gint avionic_cursor_enable(struct context *ctx, gboolean enable)
{
	GdkWindow *window = ctx->window;
	GdkCursor *cursor;

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

static JSValueRef avionic_cursor_enable_wrapper(JSContextRef context,
                                                JSObjectRef func,
                                                JSObjectRef self,
                                                size_t argc,
                                                const JSValueRef argv[],
                                                JSValueRef *exception)
{
	struct context *user_context = NULL;

	g_debug("> %s(context=%p, func=%p, self=%p, argc=%zu, argv=%p, exception=%p)",
		__func__, context, func, self, argc, argv, exception);

	if (avionic_get_user_context(context, &user_context) < 0 || !user_context) {
		g_warning("< %s() unable to get user context: false", __func__);
		return JSValueMakeBoolean(context, false);
	}

	/* check argument count, this function requires 1 parameter */
	if (argc != 1) {
		g_warning("< %s() invalid argument count %zu: false",
			__func__, argc);
		return JSValueMakeBoolean(context, false);
	}
	/* check if type is as expected */
	if (!JSValueIsBoolean(context, argv[0])) {
		g_warning("< %s() invalid argument[0] type %d: false",
			__func__, JSValueGetType(context, argv[0]));
		return JSValueMakeBoolean(context, false);
	}
	/* call the function */
	if (!avionic_cursor_enable(user_context, JSValueToBoolean(context, argv[0]))) {
		g_warning("< %s() call failed: false", __func__);
		return JSValueMakeBoolean(context, false);
	}

	g_debug("< %s() = true", __func__);
	return JSValueMakeBoolean(context, true);
}

static int register_user_context(JSGlobalContextRef ctx, gpointer data)
{
#if 0
	JSPropertyAttributes attrib;
	JSContextRef ctx;
	JSObjectRef obj;
	JSStringRef name;
	JSValueRef val;
	JSValueRef exception = NULL;

	name = JSStringCreateWithUTF8CString("ad-user-context");

	JSObjectSetProperty(ctx, obj, name, val, attrib, &exception);

	if (exception) {
		g_warning("  %s: this should not happend: %s", __func__, JSValueToStringCopy());
	}

	JSStringRelease(name);
#endif

	avionic_set_user_context(data);
	return 0;
}

struct js_user_func_def {
	const char* name;
	JSObjectCallAsFunctionCallback func;
};

static int register_user_function(JSGlobalContextRef ctx,
                                  const struct js_user_func_def *def)
{
	JSStringRef name;
	JSObjectRef func;

	name = JSStringCreateWithUTF8CString(def->name);
	if (!name)
		return -ENOMEM;

	func = JSObjectMakeFunctionWithCallback(ctx, name, def->func);
	if (!func) {
		JSStringRelease(name);
		return -ENOSYS;
	}

	JSObjectSetProperty(ctx, JSContextGetGlobalObject(ctx), name,
		func, kJSPropertyAttributeNone, NULL);

	JSStringRelease(name);
	return 0;
}

int register_user_functions(WebKitWebView *webkit, GtkWindow *window)
{
	static const struct js_user_func_def functions[] = {
		{ "avionic_cursor_set",    avionic_cursor_set_wrapper    },
		{ "avionic_cursor_enable", avionic_cursor_enable_wrapper },
//		{ "avionic_event_register_callback", avionic_event_register_callback_wrapper }
	};

	JSGlobalContextRef context;
	WebKitWebFrame *frame;
	int err = 0;
	int i;

	frame = webkit_web_view_get_main_frame(webkit);
	if (!frame) {
		g_warning("webkit_web_view_get_main_frame failed");
		return -ENOSYS;
	}

	context = webkit_web_frame_get_global_context(frame);
	if (!context) {
		g_warning("webkit_web_frame_get_global_context failed");
		return -ENOSYS;
	}

	for (i=0; i<G_N_ELEMENTS(functions); i++) {
		err = register_user_function(context, &functions[i]);
		if (err < 0)
			g_warning("failed to register: %s", functions[i].name);
	}

	register_user_context(context, GTK_WIDGET(window));

	return err < 0 ? -ENOSYS : 0;
}
