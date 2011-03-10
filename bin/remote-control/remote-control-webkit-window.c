/*
 * Copyright (C) 2011 Avionic Design GmbH
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

#include "remote-control-webkit-window.h"

G_DEFINE_TYPE(RemoteControlWebkitWindow, remote_control_webkit_window, GTK_TYPE_WINDOW);

#define REMOTE_CONTROL_WEBKIT_WINDOW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), REMOTE_CONTROL_TYPE_WEBKIT_WINDOW, RemoteControlWebkitWindowPrivate))

struct _RemoteControlWebkitWindowPrivate {
	WebKitWebView *webkit;
	GMainContext *context;
	gchar *uri;
};

enum {
	PROP_0,
	PROP_CONTEXT,
};

static void webkit_get_property(GObject *object, guint prop_id, GValue *value,
		GParamSpec *pspec)
{
	RemoteControlWebkitWindow *window = REMOTE_CONTROL_WEBKIT_WINDOW(object);
	RemoteControlWebkitWindowPrivate *priv;

	priv = REMOTE_CONTROL_WEBKIT_WINDOW_GET_PRIVATE(window);

	switch (prop_id) {
	case PROP_CONTEXT:
		g_value_set_pointer(value, priv->context);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void webkit_set_property(GObject *object, guint prop_id,
		const GValue *value, GParamSpec *pspec)
{
	RemoteControlWebkitWindow *window = REMOTE_CONTROL_WEBKIT_WINDOW(object);
	RemoteControlWebkitWindowPrivate *priv;

	priv = REMOTE_CONTROL_WEBKIT_WINDOW_GET_PRIVATE(window);

	switch (prop_id) {
	case PROP_CONTEXT:
		priv->context = g_value_get_pointer(value);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void webkit_finalize(GObject *object)
{
	RemoteControlWebkitWindow *window = REMOTE_CONTROL_WEBKIT_WINDOW(object);
	RemoteControlWebkitWindowPrivate *priv;

	priv = REMOTE_CONTROL_WEBKIT_WINDOW_GET_PRIVATE(window);

	G_OBJECT_CLASS(remote_control_webkit_window_parent_class)->finalize(object);
}

static void remote_control_webkit_window_class_init(RemoteControlWebkitWindowClass *klass)
{
	GObjectClass *object = G_OBJECT_CLASS(klass);

	g_type_class_add_private(klass, sizeof(RemoteControlWebkitWindowPrivate));

	object->get_property = webkit_get_property;
	object->set_property = webkit_set_property;
	object->finalize = webkit_finalize;

	g_object_class_install_property(object, PROP_CONTEXT,
			g_param_spec_pointer("context", "main loop context",
				"Main loop context to integrate with.",
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
				G_PARAM_STATIC_STRINGS));
}

static void on_realize(GtkWidget *widget, gpointer user_data)
{
	GdkCursor *cursor = gdk_cursor_new(GDK_BLANK_CURSOR);
	gdk_window_set_cursor(widget->window, cursor);
	gdk_cursor_unref(cursor);
}

static gboolean navigation_policy(WebKitWebView *webkit,
		WebKitWebFrame *frame, WebKitNetworkRequest *request,
		WebKitWebNavigationAction *action,
		WebKitWebPolicyDecision *decision, gpointer user_data)
{
	RemoteControlWebkitWindow *self = REMOTE_CONTROL_WEBKIT_WINDOW(user_data);
	RemoteControlWebkitWindowPrivate *priv;
	const gchar *uri;

	priv = REMOTE_CONTROL_WEBKIT_WINDOW_GET_PRIVATE(self);
	uri = webkit_network_request_get_uri(request);

	if (frame == webkit_web_view_get_main_frame(webkit)) {
		if (priv->uri && !g_str_has_prefix(uri, priv->uri))
			webkit_web_policy_decision_ignore(decision);
		else
			webkit_web_policy_decision_use(decision);
	} else {
		webkit_web_policy_decision_use(decision);
	}

	return TRUE;
}

static void remote_control_webkit_window_init(RemoteControlWebkitWindow *self)
{
	RemoteControlWebkitWindowPrivate *priv;
	GtkWindow *window = GTK_WINDOW(self);
	GdkScreen *screen;
	gint cx;
	gint cy;

	priv = REMOTE_CONTROL_WEBKIT_WINDOW_GET_PRIVATE(self);

	g_signal_connect(G_OBJECT(self), "realize",
			(GCallback)on_realize, NULL);

	screen = gtk_window_get_screen(window);
	cx = gdk_screen_get_width(screen);
	cy = gdk_screen_get_height(screen);

	gtk_widget_set_size_request(GTK_WIDGET(window), cx, cy);

	priv->webkit = WEBKIT_WEB_VIEW(webkit_web_view_new());
	gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(priv->webkit));

	g_signal_connect(G_OBJECT(priv->webkit),
			"navigation-policy-decision-requested",
			G_CALLBACK(navigation_policy), self);
}

GtkWidget *remote_control_webkit_window_new(GMainContext *context)
{
	return g_object_new(REMOTE_CONTROL_TYPE_WEBKIT_WINDOW, "context", context, NULL);
}

gboolean remote_control_webkit_window_load(RemoteControlWebkitWindow *self,
		const gchar *uri)
{
	RemoteControlWebkitWindowPrivate *priv;

	priv = REMOTE_CONTROL_WEBKIT_WINDOW_GET_PRIVATE(self);
	webkit_web_view_load_uri(priv->webkit, uri);

	if (priv->uri)
		g_free(priv->uri);

	priv->uri = g_strdup(uri);

	return TRUE;
}
