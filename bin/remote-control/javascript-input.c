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

#include <linux/input.h>

#include <gudev/gudev.h>

#include "javascript.h"
#include "find-device.h"

struct input {
	GSource source;
	GList *devices;
	GUdevClient* udev_client;

	JSContextRef context;
	JSObjectRef callback;
	JSObjectRef this;
};

static int input_report(struct input *input, struct input_event *event)
{
	JSValueRef exception = NULL;
	JSValueRef args[4];
	double timestamp;

	g_return_val_if_fail(input->context != NULL, -EINVAL);
	g_return_val_if_fail(event != NULL, -EINVAL);

	/* Input object has been used but callback has not been set */
	if (input->callback == NULL)
		return 0;

	timestamp = event->time.tv_sec + (event->time.tv_usec / 1000000.0f);

	args[0] = JSValueMakeNumber(input->context, timestamp);
	args[1] = JSValueMakeNumber(input->context, event->type);
	args[2] = JSValueMakeNumber(input->context, event->code);
	args[3] = JSValueMakeNumber(input->context, event->value);

	(void)JSObjectCallAsFunction(input->context, input->callback,
			input->this, G_N_ELEMENTS(args), args, &exception);
	if (exception) {
		g_warning("%s: exception in callback", __func__);
		return -EFAULT;
	}

	return 0;
}

static gboolean input_source_prepare(GSource *source, gint *timeout)
{
	if (timeout)
		*timeout = -1;

	return FALSE;
}

static gboolean input_source_check(GSource *source)
{
	struct input *input = (struct input *)source;
	GList *node;

	for (node = g_list_first(input->devices); node; node = node->next) {
		GPollFD *poll = node->data;

		if (poll->revents & (G_IO_IN | G_IO_ERR | G_IO_HUP))
			return TRUE;
	}

	return FALSE;
}

static void free_poll(gpointer data)
{
	GPollFD *poll = data;
	close(poll->fd);
	g_free(poll);
}

static int input_remove_device(gpointer user, GPollFD *poll)
{
	struct input *input = user;

	g_return_val_if_fail(input != NULL, -EINVAL);
	g_return_val_if_fail(poll != NULL, -EINVAL);

	g_source_remove_poll((GSource*)input, poll);
	input->devices = g_list_remove(input->devices, poll);
	free_poll(poll);

	return 0;
}

static gboolean input_source_dispatch(GSource *source, GSourceFunc callback,
		gpointer user_data)
{
	struct input *input = (struct input *)source;
	GList *node, *next;

	for (node = g_list_first(input->devices); node; node = next) {
		GPollFD *poll = node->data;
		struct input_event event;
		ssize_t err;

		next = node->next;
		if (poll->revents & (G_IO_ERR | G_IO_HUP)) {
			g_debug("js-input: input device error, closing device.");
			input_remove_device(input, poll);
		} else if (poll->revents & G_IO_IN) {
			err = read(poll->fd, &event, sizeof(event));
			if (err < 0) {
				g_debug("js-input: read(): %s", g_strerror(errno));
				continue;
			}

			err = input_report(input, &event);
			if (err < 0) {
				g_debug("js-input: input_report(): %s",
						g_strerror(-err));
				continue;
			}
		}
	}

	if (callback)
		return callback(user_data);

	return TRUE;
}

static void input_source_finalize(GSource *source)
{
	struct input *input = (struct input *)source;

	if (input->udev_client)
		g_object_unref(input->udev_client);

	g_list_free_full(input->devices, free_poll);
}

static GSourceFuncs input_source_funcs = {
	.prepare = input_source_prepare,
	.check = input_source_check,
	.dispatch = input_source_dispatch,
	.finalize = input_source_finalize,
};

static int input_add_device(gpointer user, const gchar *filename)
{
	struct input *input = user;
	GPollFD *poll;
	int fd;

	g_return_val_if_fail(input != NULL, -EINVAL);
	g_return_val_if_fail(filename != NULL, -EINVAL);

	fd = open(filename, O_RDONLY);
	if (fd < 0)
		return -errno;

	poll = g_new0(GPollFD, 1);
	if (!poll) {
		close(fd);
		return -ENOMEM;
	}

	poll->events = G_IO_IN | G_IO_HUP | G_IO_ERR;
	poll->fd = fd;

	input->devices = g_list_append(input->devices, poll);
	g_source_add_poll((GSource *)input, poll);

	return 0;
}

static void input_on_udev_event(GUdevClient *client, gchar *action,
			  GUdevDevice *udevice, gpointer user_data)
{
	struct input *input = user_data;
	const char *filename;
	GUdevDevice *parent;
	const char *pname;
	const char *name;

	if (g_strcmp0(action, "add"))
		return;

	name = g_udev_device_get_name(udevice);
	if (!g_str_has_prefix(name, "event"))
		return;

	parent = g_udev_device_get_parent(udevice);
	if (!parent)
		return;

	pname = g_udev_device_get_sysfs_attr(parent, "name");
	if (!g_strcmp0(pname, "sx8634")) {
		filename = g_udev_device_get_device_file(udevice);
		input_add_device(input, filename);
	}

	g_object_unref(parent);
	g_debug("js-input: Device added %s (parent %s)\n", name, pname);
}

static GSource *input_source_new(JSContextRef context)
{
	const gchar *const subsystems[] = { "input", NULL };
	struct input *input;
	GSource *source;

	source = g_source_new(&input_source_funcs, sizeof(*input));
	if (!source) {
		g_debug("js-input: failed to allocate memory");
		return NULL;
	}

	input = (struct input *)source;
	input->context = context;
	input->callback = NULL;
	input->devices = NULL;

	input->udev_client = g_udev_client_new(subsystems);
	if (input->udev_client)
		g_signal_connect(input->udev_client, "uevent",
				 G_CALLBACK(input_on_udev_event), input);
	else
		g_warning("js-input: failed to create udev client");

	if (find_input_devices("sx8634", input_add_device, input) < 1)
		g_debug("js-input: no sx8634 device found");

	return source;
}

static void input_initialize(JSContextRef context, JSObjectRef object)
{
	struct input *input = JSObjectGetPrivate(object);

	input->this = object;
}

static void input_finalize(JSObjectRef object)
{
	GSource *source = JSObjectGetPrivate(object);

	g_source_destroy(source);
}

static JSValueRef input_get_onevent(JSContextRef context, JSObjectRef object,
		JSStringRef name, JSValueRef *exception)
{
	struct input *input = JSObjectGetPrivate(object);
	if (!input) {
		g_warning("%s: object not valid, context changed?", __func__);
		return JSValueMakeNull(context);
	}

	return input->callback;
}

static bool input_set_onevent(JSContextRef context, JSObjectRef object,
		JSStringRef name, JSValueRef value, JSValueRef *exception)
{
	struct input *input = JSObjectGetPrivate(object);
	if (!input) {
		g_warning("%s: object not valid, context changed?", __func__);
		return false;
	}

	input->callback = JSValueToObject(context, value, exception);
	if (!input->callback) {
		g_warning("%s: failed to assign callback", __func__);
		return false;
	}
	return true;
}

static const struct {
	const char *name;
	double code;
} input_event_codes[] = {
	/* Add the generated name to code mapping list */
	#include "javascript-input-codes.c"
	{}
};

static JSValueRef input_get_event_code(JSContextRef context, JSObjectRef object,
				JSStringRef name, JSValueRef *exception)
{
	int i;

	for (i = 0; input_event_codes[i].name; i++)
		if (JSStringIsEqualToUTF8CString(
				name, input_event_codes[i].name))
			return JSValueMakeNumber(
				context, input_event_codes[i].code);

	return NULL;
}

static const JSStaticValue input_properties[] = {
	{
		.name = "onevent",
		.getProperty = input_get_onevent,
		.setProperty = input_set_onevent,
		.attributes = kJSPropertyAttributeNone,
	},
	/* Add the generated properties list */
	#include "javascript-input-properties.c"
	{}
};

static const JSClassDefinition input_classdef = {
	.className = "Input",
	.initialize = input_initialize,
	.finalize = input_finalize,
	.staticValues = input_properties,
};

static JSClassRef input_class = NULL;

int javascript_register_input_class(void)
{
	input_class = JSClassCreate(&input_classdef);
	if (!input_class) {
		g_debug("js-input: failed to create Input class");
		return -ENOMEM;
	}

	return 0;
}

int javascript_register_input(JSContextRef js, JSObjectRef parent,
			      const char *name, void *user_data)
{
	GMainLoop *loop = user_data;
	JSValueRef exception;
	JSObjectRef object;
	JSStringRef string;
	GSource *source;

	source = input_source_new(js);
	if (!source)
		return -ENOMEM;

	object = JSObjectMake(js, input_class, source);

	g_source_attach(source, g_main_loop_get_context(loop));
	g_source_unref(source);

	string = JSStringCreateWithUTF8CString(name);
	JSObjectSetProperty(js, parent, string, object, 0, &exception);
	JSStringRelease(string);

	return 0;
}
