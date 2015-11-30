/*
 * Copyright (C) 2010-2013 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <glib.h>

#include "remote-control-stub.h"
#include "remote-control.h"

#include "find-device.h"

#include <linux/input.h>

struct usb_handset {
	GSource source;
	GList *devices;
	struct event_manager *events;
};

static int event_is_hook(struct input_event *event)
{
	if (event->type == EV_KEY && event->code == KEY_MUTE)
		return 1;
#ifdef SW_PHONE_HOOK
	if (event->type == EV_SW && event->code == SW_PHONE_HOOK)
		return 1;
#endif
	return 0;
}

static int usb_handset_report(struct usb_handset *input, struct input_event *in_event)
{
	struct event event;
	ssize_t err;

	memset(&event, 0, sizeof(event));

	if(event_is_hook(in_event)) {
		event.source = EVENT_SOURCE_HOOK;

		if (in_event->value)
			event.hook.state = EVENT_HOOK_STATE_OFF;
		else
			event.hook.state = EVENT_HOOK_STATE_ON;

		err = event_manager_report(input->events, &event);
		if (err < 0)
			g_debug("usb-handset: failed to report event: %s",
				g_strerror(-err));
	}
	return 0;
}

static gboolean usb_handset_prepare(GSource *source, gint *timeout)
{
	if (timeout)
		*timeout = -1;

	return FALSE;
}

static gboolean usb_handset_check(GSource *source)
{
	struct usb_handset *input = (struct usb_handset *)source;
	GList *node;

	for (node = g_list_first(input->devices); node; node = node->next) {
		GPollFD *poll = node->data;

		if (poll->revents & G_IO_IN)
			return TRUE;
	}

	return FALSE;
}

static gboolean usb_handset_dispatch(GSource *source, GSourceFunc callback,
		gpointer user_data)
{
	struct usb_handset *input = (struct usb_handset *)source;
	GList *node = g_list_first(input->devices);

	while (node != NULL) {
		GPollFD *poll = node->data;
		GList *next = node->next;
		struct input_event event;
		ssize_t err;

		if (poll->revents & G_IO_IN) {
			err = read(poll->fd, &event, sizeof(event));
			if (err < 0) {
				int e = errno;
				g_debug("usb-handset: read(): %s", g_strerror(e));
				if (e == ENODEV) {
					g_source_remove_poll(source, poll);
					input->devices = g_list_delete_link(
							input->devices, node);
				}
			} else {
				err = usb_handset_report(input, &event);
				if (err < 0) {
					g_debug("usb-handset: input_report(): %s",
							g_strerror(-err));
				}
			}
		}
		node = next;
	}

	if (callback)
		return callback(user_data);

	return TRUE;
}

static void free_poll(gpointer data)
{
	GPollFD *poll = data;
	close(poll->fd);
	g_free(poll);
}

static void usb_handset_finalize(GSource *source)
{
	struct usb_handset *input = (struct usb_handset *)source;

	g_list_free_full(input->devices, free_poll);
}

static GSourceFuncs input_source_funcs = {
	.prepare = usb_handset_prepare,
	.check = usb_handset_check,
	.dispatch = usb_handset_dispatch,
	.finalize = usb_handset_finalize,
};

static int usb_handset_add_device(gpointer user, const gchar *filename,
		const char *name, const int productId, const int vendorId)
{
	struct usb_handset *input = user;
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

static GSource *usb_handset_new(struct remote_control *rc)
{
	struct usb_handset *input;
	GSource *source;

	source = g_source_new(&input_source_funcs, sizeof(*input));
	if (!source) {
		g_debug("usb-handset: failed to allocate memory");
		return NULL;
	}

	input = (struct usb_handset *)source;
	input->events = remote_control_get_event_manager(rc);
	input->devices = NULL;

	find_input_devices("BurrBrown from Texas Instruments USB AUDIO  CODEC",
			   usb_handset_add_device, input);

	return source;
}

int usb_handset_create(struct remote_control *rc)
{
	GSource *source;

	source = usb_handset_new(rc);
	if (!source)
		return -ENOMEM;

	g_source_attach(source, NULL);
	g_source_unref(source);

	return 0;
}
