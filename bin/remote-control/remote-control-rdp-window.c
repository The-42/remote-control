/*
 * Copyright (C) 2010-2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#ifdef HAVE_GTKX_H
#include <gtk/gtkx.h>
#endif
#include <gdk/gdkx.h>

#include "remote-control-rdp-window.h"

#define RDP_DELAY_RETRY 3

G_DEFINE_TYPE(RemoteControlRdpWindow, remote_control_rdp_window, GTK_TYPE_WINDOW);

#define REMOTE_CONTROL_RDP_WINDOW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), REMOTE_CONTROL_TYPE_RDP_WINDOW, RemoteControlRdpWindowPrivate))

struct _RemoteControlRdpWindowPrivate {
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

static void remote_control_rdp_window_get_property(GObject *object,
		guint prop_id, GValue *value, GParamSpec *pspec)
{
	RemoteControlRdpWindow *window = REMOTE_CONTROL_RDP_WINDOW(object);
	RemoteControlRdpWindowPrivate *priv;

	priv = REMOTE_CONTROL_RDP_WINDOW_GET_PRIVATE(window);

	switch (prop_id) {
	case PROP_LOOP:
		g_value_set_pointer(value, priv->loop);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void remote_control_rdp_window_set_property(GObject *object,
		guint prop_id, const GValue *value, GParamSpec *pspec)
{
	RemoteControlRdpWindow *window = REMOTE_CONTROL_RDP_WINDOW(object);
	RemoteControlRdpWindowPrivate *priv;

	priv = REMOTE_CONTROL_RDP_WINDOW_GET_PRIVATE(window);

	switch (prop_id) {
	case PROP_LOOP:
		priv->loop = g_value_get_pointer(value);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void remote_control_rdp_window_finalize(GObject *object)
{
	RemoteControlRdpWindow *window = REMOTE_CONTROL_RDP_WINDOW(object);

	remote_control_rdp_window_disconnect(window);

	G_OBJECT_CLASS(remote_control_rdp_window_parent_class)->finalize(object);
}

static void remote_control_rdp_window_class_init(RemoteControlRdpWindowClass *klass)
{
	GObjectClass *object = G_OBJECT_CLASS(klass);

	g_type_class_add_private(klass, sizeof(RemoteControlRdpWindowPrivate));

	object->get_property = remote_control_rdp_window_get_property;
	object->set_property = remote_control_rdp_window_set_property;
	object->finalize = remote_control_rdp_window_finalize;

	g_object_class_install_property(object, PROP_LOOP,
			g_param_spec_pointer("loop", "GLib main loop",
				"GLib main loop to integrate with.",
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
				G_PARAM_STATIC_STRINGS));
}

static void on_realize(GtkWidget *widget, gpointer user_data)
{
	GdkDisplay *display = gdk_display_get_default();
	GdkCursor *cursor = gdk_cursor_new_for_display(display, GDK_BLANK_CURSOR);
	GdkWindow *window = gtk_widget_get_window(widget);

	gdk_window_set_cursor(window, cursor);
#if GTK_CHECK_VERSION(2, 91, 7)
	g_object_unref(cursor);
#else
	gdk_cursor_unref(cursor);
#endif
}

static gboolean plug_removed(GtkSocket *sock, gpointer data)
{
	return TRUE;
}

static void remote_control_rdp_window_init(RemoteControlRdpWindow *self)
{
	GtkWindow *window = GTK_WINDOW(self);
	RemoteControlRdpWindowPrivate *priv;
	GdkScreen *screen;
	gint cx;
	gint cy;

	priv = REMOTE_CONTROL_RDP_WINDOW_GET_PRIVATE(self);

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

GtkWidget *remote_control_rdp_window_new(GMainLoop *loop)
{
	return g_object_new(REMOTE_CONTROL_TYPE_RDP_WINDOW, "loop", loop, NULL);
}

static gboolean reconnect(gpointer data)
{
	RemoteControlRdpWindow *self = data;
	remote_control_rdp_window_reconnect(self);
	return FALSE;
}

static gboolean start_delayed(gpointer data, guint delay)
{
	RemoteControlRdpWindow *self = data;
	RemoteControlRdpWindowPrivate *priv;
	GMainContext *context;
	GSource *timeout;

	priv = REMOTE_CONTROL_RDP_WINDOW_GET_PRIVATE(self);

	timeout = g_timeout_source_new_seconds(delay);
	g_source_set_callback(timeout, reconnect, data, NULL);
	context = g_main_loop_get_context(priv->loop);
	g_source_attach(timeout, context);

	return TRUE;
}

static void child_watch(GPid pid, gint status, gpointer data)
{
	RemoteControlRdpWindow *self = data;
	RemoteControlRdpWindowPrivate *priv;

	priv = REMOTE_CONTROL_RDP_WINDOW_GET_PRIVATE(self);

	g_source_destroy(priv->watch);
	g_spawn_close_pid(pid);

	priv->watch = NULL;
	priv->xfreerdp = 0;

	g_main_loop_quit(priv->loop);
}

gboolean remote_control_rdp_window_connect(RemoteControlRdpWindow *self,
		const gchar *hostname, const gchar *username,
		const gchar *password, guint delay)
{
	RemoteControlRdpWindowPrivate *priv;

	priv = REMOTE_CONTROL_RDP_WINDOW_GET_PRIVATE(self);

	if (priv->xfreerdp)
		remote_control_rdp_window_disconnect(self);

	priv->rdp.hostname = g_strdup(hostname);
	priv->rdp.username = g_strdup(username);
	priv->rdp.password = g_strdup(password);

	return start_delayed(self, delay);
}

gboolean remote_control_rdp_window_reconnect(RemoteControlRdpWindow *self)
{
	RemoteControlRdpWindowPrivate *priv;
	GMainContext *context;
	GError *error = NULL;
	gchar **argv;
	XID xid;

	priv = REMOTE_CONTROL_RDP_WINDOW_GET_PRIVATE(self);
	xid = gtk_socket_get_id(GTK_SOCKET(priv->socket));

	/* TODO: check for network connection (netlink socket, libnl?) */

	argv = g_new0(gchar *, 10);
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
		g_critical("g_spawn_async(): %s", error->message);
		g_error_free(error);
		g_strfreev(argv);
		return FALSE;
	}

	g_strfreev(argv);

	priv->watch = g_child_watch_source_new(priv->xfreerdp);
	if (!priv->watch) {
		g_critical("g_child_watch_source_new() failed");
		return FALSE;
	}

	g_source_set_callback(priv->watch, (GSourceFunc)child_watch, self,
			NULL);
	context = g_main_loop_get_context(priv->loop);
	g_source_attach(priv->watch, context);

	return TRUE;
}

gboolean remote_control_rdp_window_disconnect(RemoteControlRdpWindow *self)
{
	RemoteControlRdpWindowPrivate *priv;

	priv = REMOTE_CONTROL_RDP_WINDOW_GET_PRIVATE(self);

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
