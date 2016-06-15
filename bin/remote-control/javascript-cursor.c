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

#define CURSOR_MOVEMENT_TIMEOUT_MIN 0
#define CURSOR_MOVEMENT_TIMEOUT_MAX 60000

struct cursor {
	GdkDisplay *display;
	GdkScreen *screen;
	RemoteControlWebkitWindow *window;
	int uinput; /* dummy input device */
	struct cursor_movement *cursor_movement;
};

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
		g_warning("%s: failed to send event: %s",
		          __func__, g_strerror(errno));
		return -1;
	}

	return 0;
}

static int cursor_uinput_move(struct cursor *priv, int x, int y)
{
	int ret = 0;

	ret += uinput_send_event(priv->uinput, EV_ABS, ABS_X, x);
	ret += uinput_send_event(priv->uinput, EV_ABS, ABS_Y, y);
	ret += uinput_send_event(priv->uinput, EV_SYN, SYN_REPORT, 0);

	return ret < 0 ? -1 : 0;
}

static int cursor_uinput_click(struct cursor *priv)
{
	int ret = 0;

	ret += uinput_send_event(priv->uinput, EV_KEY, BTN_TOUCH, KEY_PRESSED);
	ret += uinput_send_event(priv->uinput, EV_SYN, SYN_REPORT, 0);
	ret += uinput_send_event(priv->uinput, EV_KEY, BTN_TOUCH, KEY_RELEASE);
	ret += uinput_send_event(priv->uinput, EV_SYN, SYN_REPORT, 0);

	return ret < 0 ? -1 : 0;
}

static JSValueRef cursor_moveto_callback(JSContextRef context,
                                         JSObjectRef function,
                                         JSObjectRef object,
                                         size_t argc, const JSValueRef argv[],
                                         JSValueRef *exception)
{
	struct cursor *priv = JSObjectGetPrivate(object);
	gboolean jsret = FALSE;
	int x, y, ret;

	if (!priv) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return JSValueMakeBoolean(context, FALSE);
	}

	if (argc != 2) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_ARG_COUNT);
		return JSValueMakeBoolean(context, FALSE);
	}

	if (!JSValueIsNumber(context, argv[0])) {
		javascript_set_exception_text(context, exception,
			"x is not a number");
		return JSValueMakeBoolean(context, FALSE);
	}
	if (!JSValueIsNumber(context, argv[1])) {
		javascript_set_exception_text(context, exception,
			"y is not a number");
		return JSValueMakeBoolean(context, FALSE);
	}

	x = JSValueToNumber(context, argv[0], exception);
	y = JSValueToNumber(context, argv[1], exception);

	if (priv->uinput) {
		ret = cursor_uinput_move(priv, x, y);
		if (ret) {
			javascript_set_exception_text(context, exception,
				"uinput device failure");
		} else {
			jsret = TRUE;
		}
	} else {
#if GTK_CHECK_VERSION(3, 0, 0)
		GdkDeviceManager *manager = gdk_display_get_device_manager(priv->display);
		GdkDevice *device = gdk_device_manager_get_client_pointer(manager);

		gdk_device_warp(device, priv->screen, x, y);
#else
		gdk_display_warp_pointer(priv->display, priv->screen, x, y);
#endif
		gdk_display_flush(priv->display);
		jsret = TRUE;
	}

	return JSValueMakeBoolean(context, jsret);
}

static JSValueRef cursor_clickat_callback(JSContextRef context,
                                          JSObjectRef function,
                                          JSObjectRef object,
                                          size_t argc, const JSValueRef argv[],
                                          JSValueRef *exception)
{
	struct cursor *priv = JSObjectGetPrivate(object);
	gboolean jsret = FALSE;
	GdkWindow *window;
	int x, y, ret;

	if (!priv) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return JSValueMakeBoolean(context, FALSE);
	}

	if (argc != 2) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_ARG_COUNT);
		return JSValueMakeBoolean(context, FALSE);
	}

	if (!JSValueIsNumber(context, argv[0])) {
		javascript_set_exception_text(context, exception,
			"x is not a number");
		return JSValueMakeBoolean(context, FALSE);
	}
	if (!JSValueIsNumber(context, argv[1])) {
		javascript_set_exception_text(context, exception,
			"y is not a number");
		return JSValueMakeBoolean(context, FALSE);
	}

	x = JSValueToNumber(context, argv[0], exception);
	y = JSValueToNumber(context, argv[1], exception);

	if (priv->uinput) {
		ret = cursor_uinput_move(priv, x, y);
		ret += cursor_uinput_click(priv);
		if (ret) {
			javascript_set_exception_text(context, exception,
				"uinput device failure");
		} else {
			jsret = TRUE;
		}
	} else {
		g_assert(priv->window != NULL);
		window = gtk_widget_get_window(GTK_WIDGET(GTK_WINDOW(priv->window)));
		g_assert(window != NULL);

		jsret = gdk_test_simulate_button(window, x, y, 1,
				GDK_BUTTON1_MASK, GDK_BUTTON_PRESS);
		jsret &= gdk_test_simulate_button(window, x, y, 1,
				GDK_BUTTON1_MASK, GDK_BUTTON_RELEASE);
		if (!jsret) {
			javascript_set_exception_text(context, exception,
				"gdk failure");
		}
	}

	return JSValueMakeBoolean(context, jsret);
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
	gboolean hidden;

	if (!priv) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return JSValueMakeBoolean(context, FALSE);
	}

	g_object_get(GTK_WINDOW(priv->window), "hide-cursor", &hidden, NULL);

	return JSValueMakeBoolean(context, !hidden);
}

static bool cursor_set_show(JSContextRef context, JSObjectRef object,
                            JSStringRef property, JSValueRef value,
                            JSValueRef *exception)
{
	struct cursor *priv = JSObjectGetPrivate(object);
	gboolean enable;

	if (!priv) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return false;
	}

	if (!JSValueIsBoolean(context, value)) {
		javascript_set_exception_text(context, exception,
			"not a boolean");
		return false;
	}

	enable = JSValueToBoolean(context, value);
	g_object_set(GTK_WINDOW(priv->window), "hide-cursor", !enable, NULL);

	return true;
}

static JSValueRef cursor_get_show_movement(JSContextRef context,
		JSObjectRef object, JSStringRef property, JSValueRef *exception)
{
	struct cursor *priv = JSObjectGetPrivate(object);

	if (!priv) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return JSValueMakeBoolean(context, FALSE);
	}

	return JSValueMakeNumber(context, cursor_movement_get_timeout(
			priv->cursor_movement));
}

static bool cursor_set_show_movement(JSContextRef context, JSObjectRef object,
		JSStringRef property, JSValueRef value, JSValueRef *exception)
{
	struct cursor *priv = JSObjectGetPrivate(object);
	int err, timeout;

	if (!priv) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return false;
	}

	err = javascript_int_from_number(context, value,
			CURSOR_MOVEMENT_TIMEOUT_MIN,
			CURSOR_MOVEMENT_TIMEOUT_MAX, &timeout, exception);
	if (err)
		return false;

	return cursor_movement_set_timeout(priv->cursor_movement, timeout) == 0;
}

static const JSStaticValue cursor_properties[] = {
	{
		.name = "show",
		.getProperty = cursor_get_show,
		.setProperty = cursor_set_show,
		.attributes = kJSPropertyAttributeNone,
	}, {
		.name = "showMovement",
		.getProperty = cursor_get_show_movement,
		.setProperty = cursor_set_show_movement,
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

	for (i = 0; i < G_N_ELEMENTS(DEVS); i++) {
		fd = open(DEVS[i], O_WRONLY|O_NDELAY|O_CLOEXEC);
		if (fd >= 0)
			break;
	}
	if (fd < 0) {
		g_warning("%s: no uinput device found, try: modprobe uinput",
		          __func__);
		return -ENOENT;
	}

	width = gdk_screen_get_width(priv->screen) - 1;
	height = gdk_screen_get_height(priv->screen) - 1;

	memset(&dev, 0, sizeof(dev));
	strncpy(dev.name, "Avionic Design GmbH virtual touch",
		UINPUT_MAX_NAME_SIZE);
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

	err = ioctl(fd, UI_SET_PHYS, PACKAGE_NAME"/"PACKAGE_VERSION);

	if (ioctl(fd, UI_SET_EVBIT, EV_KEY) ||
	    ioctl(fd, UI_SET_KEYBIT, BTN_TOUCH) ||
	    ioctl(fd, UI_SET_EVBIT, EV_ABS) ||
	    ioctl(fd, UI_SET_ABSBIT, ABS_X) ||
	    ioctl(fd, UI_SET_ABSBIT, ABS_Y))
	{
		g_warning("%s: setup error, ioctl failed with %s",
		          __func__, g_strerror(errno));
		goto cleanup;
	}

	/* creates device - DO NOT FORGET TO UNREGISTER */
	err = ioctl(fd, UI_DEV_CREATE);
	if (err < 0) {
		g_warning("%s: creating uinput device failed %s",
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
		g_warning("%s: destroying uinput device failed %s",
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

static JSObjectRef javascript_cursor_create(
	JSContextRef js, JSClassRef class, struct javascript_userdata *data)
{
	struct cursor *priv;

	priv = g_new0(struct cursor, 1);
	if (!priv)
		return NULL;

	priv->display = gdk_display_get_default();
	g_assert(priv->display != NULL);

	priv->screen = gdk_display_get_default_screen(priv->display);
	g_assert(priv->screen != NULL);

	priv->window = data->window;
	g_assert(priv->window != NULL);
	g_assert(REMOTE_CONTROL_IS_WEBKIT_WINDOW(priv->window));

	priv->cursor_movement = remote_control_get_cursor_movement(
			data->rcd->rc);

	return JSObjectMake(js, class, priv);
}

struct javascript_module javascript_cursor = {
	.classdef = &cursor_classdef,
	.create = javascript_cursor_create,
};
