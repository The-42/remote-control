/*
 * Copyright (C) 2010-2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include <gdk/gdkx.h>

#include "remote-control-window.h"

#define RDP_DELAY_RETRY 3

G_DEFINE_TYPE(RemoteControlWindow, remote_control_window, GTK_TYPE_WINDOW);

#define REMOTE_CONTROL_WINDOW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), REMOTE_CONTROL_TYPE_WINDOW, RemoteControlWindowPrivate))

struct _RemoteControlWindowPrivate {
	GtkWidget *socket;
	GMainLoop *loop;
	GSource *watch;
	GPid xfreerdp;

	struct {
		gchar *hostname;
		gchar *username;
		gchar *password;
	} rdp;
};

enum {
	PROP_0,
	PROP_LOOP,
};

static void remote_control_window_get_property(GObject *object, guint prop_id,
		GValue *value, GParamSpec *pspec)
{
	RemoteControlWindow *window = REMOTE_CONTROL_WINDOW(object);
	RemoteControlWindowPrivate *priv;

	priv = REMOTE_CONTROL_WINDOW_GET_PRIVATE(window);

	switch (prop_id) {
	case PROP_LOOP:
		g_value_set_pointer(value, priv->loop);
		break;

	default:
		g_assert_not_reached();
	}
}

static void remote_control_window_set_property(GObject *object, guint prop_id,
		const GValue *value, GParamSpec *pspec)
{
	RemoteControlWindow *window = REMOTE_CONTROL_WINDOW(object);
	RemoteControlWindowPrivate *priv;

	priv = REMOTE_CONTROL_WINDOW_GET_PRIVATE(window);

	switch (prop_id) {
	case PROP_LOOP:
		priv->loop = g_value_get_pointer(value);
		break;

	default:
		g_assert_not_reached();
	}
}

static void remote_control_window_finalize(GObject *object)
{
	RemoteControlWindow *window = REMOTE_CONTROL_WINDOW(object);
	RemoteControlWindowPrivate *priv;

	priv = REMOTE_CONTROL_WINDOW_GET_PRIVATE(window);

	remote_control_window_disconnect(window);

	G_OBJECT_CLASS(remote_control_window_parent_class)->finalize(object);
}

static void remote_control_window_class_init(RemoteControlWindowClass *klass)
{
	GObjectClass *object = G_OBJECT_CLASS(klass);

	g_type_class_add_private(klass, sizeof(RemoteControlWindowPrivate));

	object->get_property = remote_control_window_get_property;
	object->set_property = remote_control_window_set_property;
	object->finalize = remote_control_window_finalize;

	g_object_class_install_property(object, PROP_LOOP,
			g_param_spec_pointer("loop", "application main loop",
				"Application main loop to integrate with.",
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
				G_PARAM_STATIC_STRINGS));
}

static void on_realize(GtkWidget *widget, gpointer user_data)
{
	GdkCursor *cursor = gdk_cursor_new(GDK_BLANK_CURSOR);
	gdk_window_set_cursor(widget->window, cursor);
	gdk_cursor_unref(cursor);
}

static gboolean plug_removed(GtkSocket *sock, gpointer data)
{
	return TRUE;
}

static void remote_control_window_init(RemoteControlWindow *self)
{
	GtkWindow *window = GTK_WINDOW(self);
	RemoteControlWindowPrivate *priv;
	GdkScreen *screen;
	gint cx;
	gint cy;

	priv = REMOTE_CONTROL_WINDOW_GET_PRIVATE(self);

	g_signal_connect(G_OBJECT(self), "realize",
			(GCallback)on_realize, NULL);

	screen = gtk_window_get_screen(window);
	cx = gdk_screen_get_width(screen);
	cy = gdk_screen_get_height(screen);

	gtk_widget_set_size_request(GTK_WIDGET(window), cx, cy);

	priv->socket = gtk_socket_new();
	g_signal_connect(G_OBJECT(priv->socket), "plug-removed",
			(GCallback)plug_removed, NULL);
	gtk_container_add(GTK_CONTAINER(window), priv->socket);
}

GtkWidget *remote_control_window_new(GMainLoop *loop)
{
	return g_object_new(REMOTE_CONTROL_TYPE_WINDOW, "loop", loop, NULL);
}

static gboolean reconnect(gpointer data)
{
	RemoteControlWindow *self = data;
	remote_control_window_reconnect(self);
	return FALSE;
}

static gboolean start_delayed(gpointer data, guint delay)
{
	RemoteControlWindow *self = data;
	RemoteControlWindowPrivate *priv;
	GMainContext *context;
	GSource *timeout;

	priv = REMOTE_CONTROL_WINDOW_GET_PRIVATE(self);

	context = g_main_loop_get_context(priv->loop);
	timeout = g_timeout_source_new_seconds(delay);
	g_source_set_callback(timeout, reconnect, data, NULL);
	g_source_attach(timeout, context);

	return TRUE;
}

static void child_watch(GPid pid, gint status, gpointer data)
{
	RemoteControlWindow *self = data;
	RemoteControlWindowPrivate *priv;

	priv = REMOTE_CONTROL_WINDOW_GET_PRIVATE(self);

	g_source_destroy(priv->watch);
	g_spawn_close_pid(pid);

	priv->watch = NULL;
	priv->xfreerdp = 0;

	start_delayed(data, RDP_DELAY_RETRY);
}

gboolean remote_control_window_connect(RemoteControlWindow *self,
		const gchar *hostname, const gchar *username,
		const gchar *password, guint delay)
{
	RemoteControlWindowPrivate *priv;

	priv = REMOTE_CONTROL_WINDOW_GET_PRIVATE(self);

	if (priv->xfreerdp)
		remote_control_window_disconnect(self);

	priv->rdp.hostname = g_strdup(hostname);
	priv->rdp.username = g_strdup(username);
	priv->rdp.password = g_strdup(password);

	return start_delayed(self, delay);
}

gboolean remote_control_window_reconnect(RemoteControlWindow *self)
{
	RemoteControlWindowPrivate *priv;
	GMainContext *context;
	GError *error = NULL;
	gchar **argv;
	XID xid;

	priv = REMOTE_CONTROL_WINDOW_GET_PRIVATE(self);
	xid = gtk_socket_get_id(GTK_SOCKET(priv->socket));

	/* TODO: check for network connection (netlink socket, libnl?) */

	argv = g_new0(gchar *, 10);
	if (!argv) {
		g_error("g_new0() failed");
		return FALSE;
	}

	argv[0] = g_strdup("xfreerdp");
	argv[1] = g_strdup("-u");
	argv[2] = g_strdup(priv->rdp.username);
	argv[3] = g_strdup("-p");
	argv[4] = g_strdup(priv->rdp.password);
	argv[5] = g_strdup("-X");
	argv[6] = g_strdup_printf("%lx", xid);
	argv[7] = g_strdup("--kiosk");
	argv[8] = g_strdup(priv->rdp.hostname);
	argv[9] = NULL;

	if (!g_spawn_async(NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD |
			G_SPAWN_SEARCH_PATH, NULL, NULL, &priv->xfreerdp,
			&error)) {
		g_error("g_spawn_async(): %s", error->message);
		g_error_free(error);
		g_strfreev(argv);
		return FALSE;
	}

	g_strfreev(argv);

	priv->watch = g_child_watch_source_new(priv->xfreerdp);
	if (!priv->watch) {
		g_error("g_child_watch_source_new() failed");
		return FALSE;
	}

	g_source_set_callback(priv->watch, (GSourceFunc)child_watch, self,
			NULL);
	context = g_main_loop_get_context(priv->loop);
	g_source_attach(priv->watch, context);

	return TRUE;
}

gboolean remote_control_window_disconnect(RemoteControlWindow *self)
{
	RemoteControlWindowPrivate *priv;

	priv = REMOTE_CONTROL_WINDOW_GET_PRIVATE(self);

	if (priv->xfreerdp) {
		pid_t pid = priv->xfreerdp;
		int status = 0;
		int err;

		err = kill(pid, SIGTERM);
		if (err < 0) {
			fprintf(stderr, "kill(): %s\n", strerror(errno));
			return FALSE;
		}

		err = waitpid(pid, &status, 0);
		if (err < 0) {
			fprintf(stderr, "waitpid(): %s\n", strerror(errno));
			return FALSE;
		}

		g_debug("xfreerdp exited: %d", WEXITSTATUS(status));
		g_source_destroy(priv->watch);
		g_spawn_close_pid(pid);

		priv->watch = NULL;
		priv->xfreerdp = 0;
	}

	g_free(priv->rdp.password);
	g_free(priv->rdp.username);
	g_free(priv->rdp.hostname);

	return TRUE;
}
