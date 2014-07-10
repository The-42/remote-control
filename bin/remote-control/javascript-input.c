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
#include <string.h>

#include <linux/input.h>

#include <gudev/gudev.h>

#include "javascript.h"
#include "find-device.h"

struct device {
	int vendorId;
	int productId;
	gchar *name;

	GPollFD *poll;
};

struct input {
	GSource source;
	GList *devices;
	GUdevClient* udev_client;

	JSContextRef context;
	JSObjectRef callback;
	JSObjectRef this;
};

static const gchar **supported_devices = NULL;
static int supported_devices_count = 0;

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
		struct device *device = node->data;
		GPollFD *poll = device->poll;

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

static void free_device(gpointer data)
{
	struct device *device = data;
	free_poll(device->poll);
	g_free(device->name);
	g_free(device);
}

static int input_remove_device(gpointer user, struct device *device)
{
	struct input *input = user;
	GPollFD *poll;

	g_return_val_if_fail(device != NULL, -EINVAL);
	g_return_val_if_fail(input != NULL, -EINVAL);

	poll = device->poll;
	g_source_remove_poll((GSource*)input, poll);
	input->devices = g_list_remove(input->devices, device);
	free_device(device);

	return 0;
}

static gboolean input_source_dispatch(GSource *source, GSourceFunc callback,
		gpointer user_data)
{
	struct input *input = (struct input *)source;
	GList *node, *next;

	for (node = g_list_first(input->devices); node; node = next) {
		struct device *device = node->data;
		GPollFD *poll = device->poll;
		struct input_event event;
		ssize_t err;

		next = node->next;
		if (poll->revents & (G_IO_ERR | G_IO_HUP)) {
			g_debug("js-input: input device error, closing device.");
			input_remove_device(input, device);
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

	g_list_free_full(input->devices, free_device);
}

static GSourceFuncs input_source_funcs = {
	.prepare = input_source_prepare,
	.check = input_source_check,
	.dispatch = input_source_dispatch,
	.finalize = input_source_finalize,
};

static int input_add_device(gpointer user, const gchar *filename,
		const gchar *name, int vendorId, int productId)
{
	struct input *input = user;
	struct device *device;
	GPollFD *poll;
	int fd;

	g_return_val_if_fail(input != NULL, -EINVAL);
	g_return_val_if_fail(filename != NULL, -EINVAL);

	fd = open(filename, O_RDONLY);
	if (fd < 0)
		return -errno;

	device = g_new0(struct device, 1);
	if (!device) {
		close(fd);
		return -ENOMEM;

	}
	poll = g_new0(GPollFD, 1);
	if (!poll) {
		close(fd);
		g_free(device);
		return -ENOMEM;
	}

	poll->events = G_IO_IN | G_IO_HUP | G_IO_ERR;
	poll->fd = fd;

	device->poll = poll;
	device->name = g_strdup(name);
	device->vendorId = vendorId;
	device->productId = productId;

	input->devices = g_list_append(input->devices, device);
	g_source_add_poll((GSource *)input, poll);

	return 0;
}

static void input_on_udev_event(GUdevClient *client, gchar *action,
			  GUdevDevice *udevice, gpointer user_data)
{
	struct input *input = user_data;
	const gchar **supported_name;
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
	for (supported_name = supported_devices; *supported_name != NULL; supported_name++) {
		g_debug("js-input: Check for device %s\n", *supported_name);
		if (!g_strcmp0(pname, *supported_name)) {
			filename = g_udev_device_get_device_file(udevice);
			input_add_device(input, filename, pname,
				g_udev_device_get_sysfs_attr_as_int(parent, "vendorId"),
				g_udev_device_get_sysfs_attr_as_int(parent, "productId"));
		}
	}

	g_object_unref(parent);
	g_debug("js-input: Device added %s (parent %s)\n", name, pname);
}

static GSource *input_source_new(JSContextRef context)
{
	const gchar *const subsystems[] = { "input", NULL };
	const gchar **supported_name;
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

	for (supported_name = supported_devices; *supported_name != NULL; supported_name++) {
		if (find_input_devices(*supported_name, input_add_device, input) < 1)
			g_debug("js-input: no %s device found", *supported_name);
	}

	return source;
}

static void input_initialize(JSContextRef context, JSObjectRef object)
{
	struct input *input = JSObjectGetPrivate(object);

	input->this = object;
}

static void input_finalize(JSObjectRef object)
{
	struct input *input = JSObjectGetPrivate(object);

	if (input->callback)
		JSValueUnprotect(input->context, input->callback);

	g_source_destroy(&input->source);
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

	if (input->callback)
		JSValueUnprotect(context, input->callback);

	input->callback = JSValueToObject(context, value, exception);
	if (!input->callback) {
		g_warning("%s: failed to assign callback", __func__);
		return false;
	}
	JSValueProtect(context, input->callback);
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

static JSValueRef input_get_event_name(
	JSContextRef context, JSObjectRef function, JSObjectRef object,
	size_t argc, const JSValueRef argv[], JSValueRef *exception)
{
	const char* prefix[2] = {};
	int type, code;
	int err, i, j;

	if (argc < 1 || argc > 2) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_ARG_COUNT);
		return NULL;
	}

	err = javascript_int_from_number(
		context, argv[0], 0, UINT16_MAX, &type, exception);
	if (err)
		return NULL;

	if (argc == 1) {
		prefix[0] = "EV_";
		code = type; /* We lookup the type */
	} else {
		err = javascript_int_from_number(
			context, argv[1], 0, UINT16_MAX, &code, exception);
		if (err)
			return NULL;

		switch (type) {
		case EV_SYN:
			prefix[0] = "SYN_";
			break;
		case EV_KEY:
			prefix[0] = "KEY_";
			prefix[1] = "BTN_";
			break;
		case EV_REL:
			prefix[0] = "REL_";
			break;
		case EV_ABS:
			prefix[0] = "ABS_";
			break;
		case EV_MSC:
			prefix[0] = "MSC_";
			break;
		case EV_SW:
			prefix[0] = "SW_";
			break;
		case EV_LED:
			prefix[0] = "LED_";
			break;
		case EV_SND:
			prefix[0] = "SND_";
			break;
		case EV_REP:
			prefix[0] = "REP_";
			break;
		default:
			return JSValueMakeNull(context);
		}
	}

	for (i = 0; input_event_codes[i].name; i++) {
		const char *name = input_event_codes[i].name;
		for (j = 0; prefix[j]; j++) {
			if (!strncmp(prefix[j], name, strlen(prefix[j])) &&
					code == input_event_codes[i].code)
				return javascript_make_string(
					context, name, exception);
		}
	}

	return JSValueMakeNull(context);
}

static const JSStaticFunction input_functions[] = {
	{
		.name = "getEventName",
		.callAsFunction = input_get_event_name,
		.attributes = kJSPropertyAttributeDontDelete,
	},
	{}
};

static const JSClassDefinition input_classdef = {
	.className = "Input",
	.initialize = input_initialize,
	.finalize = input_finalize,
	.staticValues = input_properties,
	.staticFunctions = input_functions,
};

static JSObjectRef javascript_input_create(
	JSContextRef js, JSClassRef class, struct javascript_userdata *data)
{
	GSource *source;

	source = input_source_new(js);
	if (!source)
		return NULL;

	g_source_attach(source, g_main_loop_get_context(data->loop));
	g_source_unref(source);

	return JSObjectMake(js, class, source);
}

static int javascript_input_init(GKeyFile *config)
{
	gchar **keys;
	gchar *name;
	int i;

	supported_devices = g_malloc0(sizeof(*supported_devices));

	keys = g_key_file_get_keys(config, "input", NULL, NULL);
	if (!keys)
		return 0;

	for (i = 0; keys[i]; i++) {
		if (!g_str_has_prefix(keys[i], "device-"))
			continue;
		name = g_key_file_get_string(config, "input", keys[i], NULL);
		if (!name) {
			g_warning("%s: failed to read input config key %s",
				__func__, keys[i]);
			continue;
		}
		g_debug("%s: adding input device '%s'", __func__, name);
		supported_devices = g_realloc_n(supported_devices,
						supported_devices_count + 2,
						sizeof(*supported_devices));
		supported_devices[supported_devices_count] = name;
		supported_devices[supported_devices_count + 1] = NULL;
		supported_devices_count++;
	}

	g_strfreev(keys);

	return 0;
}

struct javascript_module javascript_input = {
	.classdef = &input_classdef,
	.init = javascript_input_init,
	.create = javascript_input_create,
};
