/*
 * Copyright (C) 2011-2012 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <gtk/gtk.h>

#include <JavaScriptCore/JavaScript.h>

#include "remote-control-webkit-jscript.h"
#if defined(ENABLE_JAVASCRIPT_IR)
#include "remote-control-irkey.h"
#endif

struct context {
	GdkWindow *window;
#if defined(ENABLE_JAVASCRIPT_IR)
	struct irkey *irk;
#endif
};

static struct context* avionic_get_context()
{
	static struct context *ctx = NULL;
	if (ctx == NULL) {
		ctx = g_new(struct context, 1);
		memset(ctx, 0, sizeof(*ctx));
#if defined(ENABLE_JAVASCRIPT_IR)
		ctx->irk = irk_new();
		if (ctx->irk) {
			int err = irk_setup_thread(ctx->irk);
			if (err < 0)
				g_critical("%s: failed to setup irkey thread: %d",
					__func__, err);
		}
#endif
	}
	return ctx;
}

static void avionic_set_user_context(gpointer data)
{
	struct context *ctx = avionic_get_context();
	if (ctx)
		ctx->window = gtk_widget_get_window(GTK_WIDGET(data));
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

static JSValueRef avionic_cursor_set_wrapper(JSContextRef context,
                                             JSObjectRef func,
                                             JSObjectRef self,
                                             size_t argc,
                                             const JSValueRef argv[],
                                             JSValueRef *exception)
{
	struct context *user_context = NULL;
	bool ret;

	if (avionic_get_user_context(context, &user_context) < 0 || !user_context)
		return JSValueMakeBoolean(context, FALSE);

	/* check argument count, this function requires 1 parameter */
	if (argc != 2) {
		if (exception) {
			JSStringRef text = JSStringCreateWithUTF8CString("invalid argument count");
			*exception = JSValueMakeString(context, text);
			JSStringRelease(text);
		}
		return JSValueMakeBoolean(context, FALSE);
	}
	/* check if type is as expected */
	if (!JSValueIsNumber(context, argv[0]))
		return JSValueMakeBoolean(context, FALSE);
	if (!JSValueIsNumber(context, argv[1]))
		return JSValueMakeBoolean(context, FALSE);

	/* call the function */
	ret = avionic_cursor_set(user_context,
	          (gint)JSValueToNumber(context, argv[0], exception),
	          (gint)JSValueToNumber(context, argv[1], exception));
	if (!ret)
		return JSValueMakeBoolean(context, FALSE);

	return JSValueMakeBoolean(context, TRUE);
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

	if (avionic_get_user_context(context, &user_context) < 0 || !user_context)
		return JSValueMakeBoolean(context, FALSE);

	/* check argument count, this function requires 1 parameter */
	if (argc != 1)
		return JSValueMakeBoolean(context, FALSE);

	/* check if type is as expected */
	if (!JSValueIsBoolean(context, argv[0]))
		return JSValueMakeBoolean(context, FALSE);

	/* call the function */
	if (!avionic_cursor_enable(user_context, JSValueToBoolean(context, argv[0])))
		return JSValueMakeBoolean(context, FALSE);

	return JSValueMakeBoolean(context, TRUE);
}

static gint avionic_cursor_click(struct context *ctx, gint x, gint y)
{
	gdk_test_simulate_button(ctx->window, x, y, 1, GDK_BUTTON1_MASK, GDK_BUTTON_PRESS);
	gdk_test_simulate_button(ctx->window, x, y, 1, GDK_BUTTON1_MASK, GDK_BUTTON_RELEASE);

	return 0;
}

static JSValueRef avionic_cursor_click_wrapper(JSContextRef context,
                                               JSObjectRef func,
                                               JSObjectRef self,
                                               size_t argc,
                                               const JSValueRef argv[],
                                               JSValueRef *exception)
{
	struct context *user_context = NULL;
	gint ret;

	if (avionic_get_user_context(context, &user_context) < 0 || !user_context)
		return JSValueMakeBoolean(context, FALSE);

	/* check argument count, this function requires 1 parameter */
	if (argc != 2) {
		if (exception) {
			JSStringRef text = JSStringCreateWithUTF8CString("invalid argument count");
			*exception = JSValueMakeString(context, text);
			JSStringRelease(text);
		}
		return JSValueMakeBoolean(context, FALSE);
	}
	/* check if type is as expected */
	if (!JSValueIsNumber(context, argv[0]))
		return JSValueMakeBoolean(context, FALSE);
	if (!JSValueIsNumber(context, argv[1]))
		return JSValueMakeBoolean(context, FALSE);

	/* call the function */
	ret = avionic_cursor_click(user_context,
	          (gint)JSValueToNumber(context, argv[0], exception),
	          (gint)JSValueToNumber(context, argv[1], exception));
	if (!ret)
		return JSValueMakeBoolean(context, FALSE);

	return JSValueMakeBoolean(context, TRUE);
}

static gint avionic_ir_get_message(struct context *ctx, JSStringRef *str)
{
#if defined(ENABLE_JAVASCRIPT_IR)
	struct ir_message *msg = NULL;
	gchar text[24];
	int err;

	if (!ctx || !str)
		return -EINVAL;

	err = irk_peek_message(ctx->irk, &msg);
	if (err < 0 || msg == NULL) {
		if (msg)
			g_free(msg);
		return -ENODATA;
	}

	g_snprintf(text, G_N_ELEMENTS(text), "%02x %02x %02x %02x %02x %02x %02x %02x",
		msg->header, msg->reserved, msg->d0, msg->d1, msg->d2,
		msg->d3, msg->d4, msg->d5);
	g_free(msg);

	g_debug("   got: [%s]", text);
	*str = JSStringCreateWithUTF8CString(text);
	return 0;
#else
	return -ENOSYS;
#endif
}

static JSValueRef avionic_ir_get_msg_wrapper(JSContextRef context,
                                             JSObjectRef func,
                                             JSObjectRef self,
                                             size_t argc,
                                             const JSValueRef argv[],
                                             JSValueRef *exception)
{
	struct context *user_context = NULL;
	JSStringRef str = NULL;
	JSValueRef val = NULL;
	gint ret;

	/* check argument count, this function requires 1 parameter */
	if (argc != 0)
		return JSValueMakeNull(context);

	/* get and verify the user context */
	if (avionic_get_user_context(context, &user_context) < 0 || !user_context)
		return JSValueMakeNull(context);

	ret = avionic_ir_get_message(user_context, &str);
	if (ret < 0)
		return JSValueMakeNull(context);

	val = JSValueMakeString(context, str);
	JSStringRelease(str);

	return val;
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

int register_user_functions(WebKitWebView *webkit, GtkWidget *widget)
{
	static const struct js_user_func_def functions[] = {
		{ "avionic_cursor_set",     avionic_cursor_set_wrapper    },
		{ "avionic_cursor_enable",  avionic_cursor_enable_wrapper },
		{ "avionic_cursor_click",   avionic_cursor_click_wrapper  },
		{ "avionic_ir_get_message", avionic_ir_get_msg_wrapper    },
//		{ "avionic_event_register_callback", avionic_event_register_callback_wrapper }
	};

	JSGlobalContextRef context;
	WebKitWebFrame *frame;
	int err = 0;
	int i;

	frame = webkit_web_view_get_main_frame(webkit);
	if (!frame) {
		g_warning("%s: webkit_web_view_get_main_frame failed", __func__);
		return -ENOSYS;
	}

	context = webkit_web_frame_get_global_context(frame);
	if (!context) {
		g_warning("%s: webkit_web_frame_get_global_context failed", __func__);
		return -ENOSYS;
	}

	for (i=0; i<G_N_ELEMENTS(functions); i++) {
		err = register_user_function(context, &functions[i]);
		if (err < 0) {
			g_warning("%s: failed to register: %s",
				__func__, functions[i].name);
		}
	}

	register_user_context(context, widget);

	return err < 0 ? -ENOSYS : 0;
}
