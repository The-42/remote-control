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
#include <string.h>
#include <unistd.h>

#include <linux/uinput.h>
#include <gtk/gtk.h>

#include "javascript.h"

#define KEY_PRESSED 1
#define KEY_RELEASE 0

struct cursor {
	GdkDisplay *display;
	GdkScreen *screen;
	RemoteControlWebkitWindow *window;
	int uinput; /* dummy input device */
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

static inline int uinput_send_event(int uinput, int type, int code, int value)
{
	struct input_event event;
	int ret;

	memset(&event, 0, sizeof(event));
	event.type  = type;
	event.code  = code;
	event.value = value;

	ret = write(uinput, &event, sizeof(event));
	if (ret != sizeof(event)) {
		g_warning("%s: failed to send event %s",
		          __func__, g_strerror(errno));
		return -1;
	}

	return 0;
}

static void cursor_uinput_move(struct cursor *priv, int x, int y)
{
	uinput_send_event(priv->uinput, EV_ABS, ABS_X, x);
	uinput_send_event(priv->uinput, EV_ABS, ABS_Y, y);
	uinput_send_event(priv->uinput, EV_SYN, SYN_REPORT, 0);
}

static void cursor_uinput_click(struct cursor *priv)
{
	uinput_send_event(priv->uinput, EV_KEY, BTN_TOUCH, KEY_PRESSED);
	uinput_send_event(priv->uinput, EV_SYN, SYN_REPORT, 0);
	uinput_send_event(priv->uinput, EV_KEY, BTN_TOUCH, KEY_RELEASE);
	uinput_send_event(priv->uinput, EV_SYN, SYN_REPORT, 0);
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

	if (priv->uinput) {
		cursor_uinput_move(priv, x, y);
	} else {
		g_assert(priv->display != NULL);
		g_assert(priv->screen != NULL);
		gdk_display_warp_pointer(priv->display, priv->screen, x, y);
		gdk_display_flush(priv->display);
	}

	return JSValueMakeBoolean(context, TRUE);
}

static JSValueRef cursor_clickat_callback(JSContextRef context,
                                          JSObjectRef function,
                                          JSObjectRef object,
                                          size_t argc, const JSValueRef argv[],
                                          JSValueRef *exception)
{
	struct cursor *priv = JSObjectGetPrivate(object);
	GdkWindow *window;
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

	if (priv->uinput) {
		cursor_uinput_move(priv, x, y);
		cursor_uinput_click(priv);
	} else {
		g_assert(priv->window != NULL);
		window =  gtk_widget_get_window(GTK_WIDGET(GTK_WINDOW(priv->window)));
		g_assert(window != NULL);

		gdk_test_simulate_button(window, x, y, 1, GDK_BUTTON1_MASK,
		                         GDK_BUTTON_PRESS);
		gdk_test_simulate_button(window, x, y, 1, GDK_BUTTON1_MASK,
		                         GDK_BUTTON_RELEASE);
	}

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

/**
 * Create a dummy input device. This is needed to make cursor movement work
 * if no input device (like mouse) is attached.
 */
static int cursor_uinput_create(struct cursor *priv)
{
	static const char *DEVS[] = {
		"/dev/uinput",
		"/dev/input/uinput"
	};
	struct uinput_user_dev dev;
	gint width, height;
	int err, fd, i;

	for (i=0; i<G_N_ELEMENTS(DEVS); i++) {
		fd = open(DEVS[i], O_WRONLY|O_NDELAY|O_CLOEXEC);
		if (fd < 0) {
			g_debug("%s: unable to open %s", __func__, DEVS[i]);
			continue;
		}
		break;
	}
	if (fd < 0) {
		g_warning("%s: no uinput device found, try: modprobe uinput",
		          __func__);
		return -ENOENT;
	}

	fcntl (fd, F_SETFD, FD_CLOEXEC);

	width = gdk_screen_get_width(priv->screen) - 1;
	height = gdk_screen_get_height(priv->screen) - 1;

	memset(&dev, 0, sizeof(dev));
	strncpy(dev.name, "Avionic Design GmbH virtual touch", UINPUT_MAX_NAME_SIZE);
	dev.id.bustype = BUS_VIRTUAL;
	dev.absmax[ABS_X] = width;
	dev.absmax[ABS_Y] = height;
	dev.absmax[ABS_MT_POSITION_X] = width;
	dev.absmax[ABS_MT_POSITION_Y] = height;
	dev.absmax[ABS_MT_TOUCH_MAJOR] = 1;

	/* setting up device info */
	err = write(fd, &dev, sizeof(dev));
	if (err != sizeof(dev)) {
		g_warning("%s: setup error, write failed with %s",
		          __func__, g_strerror(-err));
		goto cleanup;
	}

	err = ioctl (fd, UI_SET_PHYS, PACKAGE_NAME"/"PACKAGE_VERSION);

	if (ioctl(fd, UI_SET_EVBIT, EV_KEY) ||
	    ioctl(fd, UI_SET_KEYBIT, BTN_TOUCH) ||
	    ioctl(fd, UI_SET_EVBIT, EV_ABS) ||
	    ioctl(fd, UI_SET_ABSBIT, ABS_X) ||
	    ioctl(fd, UI_SET_ABSBIT, ABS_Y))
	{
		g_warning("%s: setup error, ioctrl failed %s",
		          __func__, g_strerror(errno));
		goto cleanup;
	}

	/* "creating device; DO NOT FORGET UNREGISTER */
	err = ioctl(fd, UI_DEV_CREATE);
	if (err < 0) {
		g_warning("%s: create uinput device failed %s",
		          __func__, g_strerror(-err));
		goto cleanup;
	}

	priv->uinput = fd;
	return 0;

cleanup:
	if (fd)
		close(fd);

	return -ENOENT;
}

static void cursor_uinput_destroy(struct cursor *priv)
{
	int err;

	if (!priv->uinput)
		return;

	err = ioctl(priv->uinput, UI_DEV_DESTROY);
	if (err < 0) {
		g_warning("%s: create uinput device failed %s",
		          __func__, g_strerror(-err));
	}

	close(priv->uinput);
	priv->uinput = 0;
}

static void cursor_initialize(JSContextRef context, JSObjectRef object)
{
	struct cursor *priv = JSObjectGetPrivate(object);

	cursor_uinput_create(priv);
}

static void cursor_finalize(JSObjectRef object)
{
	struct cursor *priv = JSObjectGetPrivate(object);

	cursor_uinput_destroy(priv);
	JSObjectSetPrivate(object, NULL);
	g_free(priv);
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

int javascript_register_cursor(JSContextRef js, JSObjectRef parent,
                               const char *name, void *user_data)
{
	JSValueRef exception = NULL;
	JSObjectRef object;
	JSStringRef string;
	struct cursor *priv;

	priv = g_new0(struct cursor, 1);
	if (!priv)
		return -ENOMEM;

	priv->display = gdk_display_get_default();
	g_assert(priv->display != NULL);

	priv->screen = gdk_display_get_default_screen(priv->display);
	g_assert(priv->screen != NULL);

	priv->window = user_data;
	g_assert(priv->window != NULL);
	g_assert(REMOTE_CONTROL_IS_WEBKIT_WINDOW(priv->window));

	object = JSObjectMake(js, cursor_class, priv);

	string = JSStringCreateWithUTF8CString(name);
	JSObjectSetProperty(js, parent, string, object, 0, &exception);
	JSStringRelease(string);

	return 0;
}
