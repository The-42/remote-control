/*
 * Copyright (C) 2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <ptip.h>

#include "remote-control.h"

struct handset {
	struct ptip_client *client;
	struct remote_control *rc;
	GThread *thread;
	gboolean done;
};

static int handset_handle_event(struct handset *handset)
{
	struct ptip_event *event = NULL;
	struct event_manager *events;
	struct event report;
	int err;

	events = remote_control_get_event_manager(handset->rc);

	err = ptip_client_recv(handset->client, &event);
	if (err <= 0) {
		if (err == 0) {
			g_warning("%s(): server died", __func__);
			return -ECONNRESET;
		}

		g_warning("%s(): ptip_client_recv(): %s", __func__,
				g_strerror(-err));
		return err;
	}

	memset(&report, 0, sizeof(report));
	report.source = EVENT_SOURCE_HANDSET;
	report.handset.keycode = event->keycode;

	if (event->flags & PTIP_EVENT_PRESS)
		report.handset.pressed = true;

	g_debug("PT-IP: key event: %04x %s", event->keycode,
			(event->flags & PTIP_EVENT_PRESS) ?
				"pressed" : "released");

	err = event_manager_report(events, &report);
	if (err < 0)
		g_debug("PT-IP: failed to report event: %s",
			g_strerror(-err));

	ptip_event_free(event);
	return 0;
}

static gpointer handset_thread(gpointer data)
{
	struct handset *handset = data;

	while (!handset->done) {
		struct ptip_connection *events = NULL;
		GPollFD fds[1];
		gint err;

		err = ptip_client_get_events(handset->client, &events);
		if (err < 0) {
			g_warning("%s(): ptip_client_get_events(): %s",
					__func__, g_strerror(-err));
			break;
		}

		err = ptip_connection_get_fd(events, &fds[0].fd);
		if (err < 0) {
			g_warning("%s(): ptip_connection_get_fd(): %s",
					__func__, g_strerror(-err));
			break;
		}

		fds[0].events = G_IO_IN;

		err = g_poll(fds, G_N_ELEMENTS(fds), 250);
		if (err <= 0) {
			if ((err == 0) || (errno == EINTR))
				continue;

			g_warning("%s(): g_poll(): %s", __func__,
					g_strerror(errno));
			break;
		}

		if (fds[0].revents & G_IO_IN) {
			err = handset_handle_event(handset);
			if (err < 0) {
				g_warning("%s(): handset_handle_event(): %s",
						__func__, g_strerror(-err));
				break;
			}
		}
	}

	return NULL;
}

int handset_create(struct handset **handsetp, struct remote_control *rc)
{
	struct handset *handset;
	int err;

	handset = g_new0(struct handset, 1);
	if (!handset)
		return -ENOMEM;

	err = ptip_client_create(&handset->client, "ptip");
	if (err < 0) {
		g_debug("PT-IP: connection failed: %s", g_strerror(-err));
		handset->client = NULL;
		goto out;
	}

	g_debug("PT-IP: connection established");

	handset->rc = rc;
	handset->done = FALSE;

#if !GLIB_CHECK_VERSION(2, 31, 0)
	handset->thread = g_thread_create(handset_thread, handset, TRUE,
			NULL);
#else
	handset->thread = g_thread_new("handset-ptip", handset_thread,
			handset);
#endif
	if (!handset->thread) {
		ptip_client_free(handset->client);
		g_free(handset);
		return -ENOMEM;
	}

out:
	*handsetp = handset;
	return 0;
}

int handset_free(struct handset *handset)
{
	if (!handset)
		return -EINVAL;

	if (handset->thread) {
		handset->done = TRUE;
		g_thread_join(handset->thread);
	}

	ptip_client_free(handset->client);
	g_free(handset);
	return 0;
}

int handset_display_clear(struct handset *handset)
{
	int err;

	if (!handset || !handset->client)
		return handset ? -ENODEV : -EINVAL;

	err = ptip_client_display_clear(handset->client);
	if (err < 0)
		return err;

	return 0;
}

int handset_display_sync(struct handset *handset)
{
	int err;

	if (!handset || !handset->client)
		return handset ? -ENODEV : -EINVAL;

	err = ptip_client_display_flush(handset->client);
	if (err < 0)
		return err;

	return 0;
}

int handset_display_set_brightness(struct handset *handset,
		unsigned int brightness)
{
	int err;

	if (!handset || !handset->client)
		return handset ? -ENODEV : -EINVAL;

	err = ptip_client_display_set_backlight(handset->client, brightness);
	if (err < 0)
		return err;

	return 0;
}

int handset_keypad_set_brightness(struct handset *handset,
		unsigned int brightness)
{
	int err;

	if (!handset || !handset->client)
		return handset ? -ENODEV : -EINVAL;

	err = ptip_client_keypad_set_backlight(handset->client, brightness);
	if (err < 0)
		return err;

	return 0;
}

int handset_icon_show(struct handset *handset, unsigned int id, bool show)
{
	int err;

	if (!handset || !handset->client)
		return handset ? -ENODEV : -EINVAL;

	if (show)
		err = ptip_client_icon_show(handset->client, id);
	else
		err = ptip_client_icon_hide(handset->client, id);

	if (err < 0)
		return err;

	return 0;
}

int handset_text_show(struct handset *handset, unsigned int x, unsigned int y,
		const char *text, bool show)
{
	int err;

	if (!handset || !handset->client)
		return handset ? -ENODEV : -EINVAL;

	if (show)
		err = ptip_client_text_show(handset->client, x, y, text);
	else
		err = ptip_client_text_hide(handset->client, x, y, text);

	if (err < 0)
		return err;

	return 0;
}
