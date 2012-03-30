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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "remote-control-webkit-window.h"
#include "javascript.h"
#include "utils.h"
#include "guri.h"

G_DEFINE_TYPE(RemoteControlWebkitWindow, remote_control_webkit_window, GTK_TYPE_WINDOW);

#define REMOTE_CONTROL_WEBKIT_WINDOW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), REMOTE_CONTROL_TYPE_WEBKIT_WINDOW, RemoteControlWebkitWindowPrivate))

#define WEBKIT_RELOAD_TIMEOUT 3

struct _RemoteControlWebkitWindowPrivate {
	WebKitWebView *webkit;
	GMainLoop *loop;
	GURI *uri;
	gboolean hide_cursor;
	gboolean inspector;
	/* used only when inspector enabled */
	WebKitWebInspector *webkit_inspector;
	WebKitWebView *webkit_inspector_view;
	GtkExpander *expander;
	GtkVBox *vbox;
};

enum {
	PROP_0,
	PROP_LOOP,
	PROP_CONTEXT,
	PROP_CURSOR,
	PROP_INSPECTOR
};

static gboolean webkit_cursor_is_visible(GtkWidget *widget)
{
	GdkWindow *window;
	GdkCursor *cursor;

	window = gtk_widget_get_window(widget);
	if (!window)
		return false;

	cursor = gdk_window_get_cursor(window);
	if (!cursor)
		return false;

	return gdk_cursor_get_cursor_type(cursor) != GDK_BLANK_CURSOR;
}

static void webkit_cursor_hide(GtkWidget *widget, gboolean hide)
{
	GdkCursor *cursor = gdk_cursor_new(hide ? GDK_BLANK_CURSOR : GDK_X_CURSOR);
	GdkWindow *window = gtk_widget_get_window(widget);

	gdk_window_set_cursor(window, cursor);
#if GTK_CHECK_VERSION(2, 91, 7)
	g_object_unref(cursor);
#else
	gdk_cursor_unref(cursor);
#endif
}

static void remote_control_webkit_construct_view(RemoteControlWebkitWindow *self);

static void webkit_get_property(GObject *object, guint prop_id, GValue *value,
		GParamSpec *pspec)
{
	RemoteControlWebkitWindow *window = REMOTE_CONTROL_WEBKIT_WINDOW(object);
	RemoteControlWebkitWindowPrivate *priv;

	priv = REMOTE_CONTROL_WEBKIT_WINDOW_GET_PRIVATE(window);

	switch (prop_id) {
	case PROP_LOOP:
		g_value_set_pointer(value, priv->loop);
		break;

	case PROP_CURSOR:
		g_value_set_boolean(value, !webkit_cursor_is_visible(
		                            GTK_WIDGET(GTK_WINDOW(window))));
		break;

	case PROP_INSPECTOR:
		g_value_set_boolean(value, priv->inspector);
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
	case PROP_LOOP:
		priv->loop = g_value_get_pointer(value);
		break;

	case PROP_CURSOR:
		webkit_cursor_hide(GTK_WIDGET(GTK_WINDOW(window)),
		                   g_value_get_boolean(value));
		break;

	case PROP_INSPECTOR:
		priv->inspector = g_value_get_boolean(value);
		remote_control_webkit_construct_view(window);
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
	g_object_unref(priv->uri);

	G_OBJECT_CLASS(remote_control_webkit_window_parent_class)->finalize(object);
}

#ifdef ENABLE_JAVASCRIPT
static void webkit_on_notify_load_status(WebKitWebView *webkit,
                                         GParamSpec *pspec, gpointer data)
{
	RemoteControlWebkitWindow *window = REMOTE_CONTROL_WEBKIT_WINDOW(data);
	RemoteControlWebkitWindowPrivate *priv =
	                REMOTE_CONTROL_WEBKIT_WINDOW_GET_PRIVATE(window);
	WebKitLoadStatus status;

	status = webkit_web_view_get_load_status(webkit);
	if (status == WEBKIT_LOAD_COMMITTED) {
		WebKitWebFrame *frame = webkit_web_view_get_main_frame(webkit);
		struct javascript_userdata user;
		JSGlobalContextRef context;
		int err;

		user.loop =  priv->loop;
		user.window = window;

		context = webkit_web_frame_get_global_context(frame);
		g_assert(context != NULL);

		err = javascript_register(context, &user);
		if (err < 0) {
			g_debug("failed to register JavaScript API: %s",
					g_strerror(-err));
		}
	}
}
#endif

static void remote_control_webkit_window_class_init(RemoteControlWebkitWindowClass *klass)
{
	GObjectClass *object = G_OBJECT_CLASS(klass);

	g_type_class_add_private(klass, sizeof(RemoteControlWebkitWindowPrivate));

	object->get_property = webkit_get_property;
	object->set_property = webkit_set_property;
	object->finalize = webkit_finalize;

	g_object_class_install_property(object, PROP_LOOP,
			g_param_spec_pointer("loop", "GLib main loop",
				"GLib main loop to integrate with.",
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
				G_PARAM_STATIC_STRINGS));
	g_object_class_install_property(object, PROP_CURSOR,
			g_param_spec_boolean("hide-cursor", "hide the cursor",
				"Hide/show the cursor on this page.", TRUE,
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(object, PROP_INSPECTOR,
			g_param_spec_boolean("inspector", "activate inspector",
				"Enable this to show an inspector widget.", false,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
				G_PARAM_STATIC_STRINGS));
}

static void on_realize(GtkWidget *widget, gpointer user_data)
{
	g_object_set(widget, "hide-cursor", true, NULL);
}

static gboolean navigation_policy(WebKitWebView *webkit,
		WebKitWebFrame *frame, WebKitNetworkRequest *request,
		WebKitWebNavigationAction *action,
		WebKitWebPolicyDecision *decision, gpointer user_data)
{
	RemoteControlWebkitWindow *self = REMOTE_CONTROL_WEBKIT_WINDOW(user_data);
	RemoteControlWebkitWindowPrivate *priv;
	const gchar *s;
	GURI *uri;

	priv = REMOTE_CONTROL_WEBKIT_WINDOW_GET_PRIVATE(self);
	s = webkit_network_request_get_uri(request);
	uri = g_uri_new(s);

	if (frame == webkit_web_view_get_main_frame(webkit)) {
		if (priv->uri && !g_uri_same_origin(uri, priv->uri))
			webkit_web_policy_decision_ignore(decision);
		else
			webkit_web_policy_decision_use(decision);
	} else {
		webkit_web_policy_decision_use(decision);
	}

	g_object_unref(uri);
	return TRUE;
}

static gboolean remote_control_webkit_reload(gpointer user_data)
{
	webkit_web_frame_reload(WEBKIT_WEB_FRAME(user_data));
	return FALSE;
}

static gboolean webkit_handle_load_error(WebKitWebView *webkit,
		WebKitWebFrame *frame, const gchar *uri, GError *error,
		gpointer user_data)
{
	gboolean need_reload = FALSE;

	g_debug("%s(): %s: %d: %s (%s)", __func__,
			g_quark_to_string(error->domain),
			error->code, error->message, uri);

	if (error->domain == WEBKIT_POLICY_ERROR) {
		if (error->code == WEBKIT_POLICY_ERROR_CANNOT_SHOW_URL)
			need_reload = TRUE;
	}

	if (error->domain == SOUP_HTTP_ERROR)
		need_reload = TRUE;

	if (need_reload) {
		g_debug("%s(): scheduling reload of %s...", __func__, uri);
		g_timeout_add_seconds(WEBKIT_RELOAD_TIMEOUT,
				remote_control_webkit_reload, frame);
	}

	return FALSE;
}

static void remote_control_webkit_expander_callback (
		GObject *object, GParamSpec *param_spec,
		RemoteControlWebkitWindow *self)
{
	RemoteControlWebkitWindowPrivate *priv;
	priv = REMOTE_CONTROL_WEBKIT_WINDOW_GET_PRIVATE(self);

	if (gtk_expander_get_expanded (priv->expander)) {
		gint x, y;

		x = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (priv->webkit), "x"));
		y = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (priv->webkit), "y"));
		webkit_web_inspector_inspect_coordinates (priv->webkit_inspector, x, y);
		webkit_web_inspector_show (priv->webkit_inspector);
	}
}

static void remote_control_webkit_construct_view(RemoteControlWebkitWindow *self)
{
	RemoteControlWebkitWindowPrivate *priv;
	GtkWindow *window = GTK_WINDOW(self);

	priv = REMOTE_CONTROL_WEBKIT_WINDOW_GET_PRIVATE(self);
	GtkWidget *view;

	g_print("construct view\n");
	if(priv->inspector) {
		view = gtk_scrolled_window_new(NULL, NULL);
		gtk_container_add(GTK_CONTAINER(view), GTK_WIDGET(priv->webkit));
		gtk_widget_show(view);

		WebKitWebSettings *settings =
				webkit_web_view_get_settings (WEBKIT_WEB_VIEW(priv->webkit));
		g_object_set (G_OBJECT(settings), "enable-developer-extras", TRUE,
					  NULL);

		priv->vbox = GTK_VBOX(gtk_vbox_new(false, 0));

		priv->expander = GTK_EXPANDER(gtk_expander_new("Inspector"));
		g_signal_connect (priv->expander, "notify::expanded",
						  G_CALLBACK(remote_control_webkit_expander_callback),
						  self);

		gtk_container_add(GTK_CONTAINER(priv->vbox), GTK_WIDGET(view));
		gtk_container_add(GTK_CONTAINER(priv->vbox), GTK_WIDGET(priv->expander));

		gtk_box_set_child_packing(GTK_BOX(priv->vbox),
								  GTK_WIDGET(priv->webkit),
								  true, /*expand*/
								  true, /*fill*/
								  0, /*padding*/
								  GTK_PACK_START);

		gtk_box_set_child_packing(GTK_BOX(priv->vbox),
								  GTK_WIDGET(priv->expander),
								  false, /*expand*/
								  false, /*fill*/
								  0, /*padding*/
								  GTK_PACK_START);

		gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(priv->vbox));
	} else {
		gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(priv->webkit));
	}
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

	soup_session_set_proxy(webkit_get_default_session());

	priv->webkit = WEBKIT_WEB_VIEW(webkit_web_view_new());
	gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(priv->webkit));
#ifdef ENABLE_JAVASCRIPT
	/*
	 * Add a callback to listen for load-status property changes. This is
	 * used to register the JavaScript binding within the frame.
	 */
	g_signal_connect(GTK_WIDGET(priv->webkit), "notify::load-status",
		G_CALLBACK(webkit_on_notify_load_status), self);
#endif
	g_signal_connect(G_OBJECT(priv->webkit),
			"navigation-policy-decision-requested",
			G_CALLBACK(navigation_policy), self);
	g_signal_connect(G_OBJECT(priv->webkit), "load-error",
			G_CALLBACK(webkit_handle_load_error), self);
}

GtkWidget *remote_control_webkit_window_new(GMainLoop *loop, gboolean inspector)
{
	return g_object_new(REMOTE_CONTROL_TYPE_WEBKIT_WINDOW, "loop", loop,
						"inspector", inspector, NULL);
}

static WebKitWebView* remote_control_webkit_inspector_create (
		gpointer inspector, WebKitWebView* web_view,
		gpointer data)
{
	RemoteControlWebkitWindow *self = data;
	RemoteControlWebkitWindowPrivate *priv;

	priv = REMOTE_CONTROL_WEBKIT_WINDOW_GET_PRIVATE(self);

	priv->webkit_inspector_view = WEBKIT_WEB_VIEW(webkit_web_view_new());
	gtk_container_add(GTK_CONTAINER(priv->expander),
					 GTK_WIDGET(priv->webkit_inspector_view));
	gtk_widget_show_all(GTK_WIDGET(priv->expander));

	return priv->webkit_inspector_view;
}

static gboolean remote_control_webkit_show_inpector (
		gpointer inspector, gpointer data)
{
	RemoteControlWebkitWindow *self = data;
	RemoteControlWebkitWindowPrivate *priv;

	priv = REMOTE_CONTROL_WEBKIT_WINDOW_GET_PRIVATE(self);

	gtk_widget_set_size_request(GTK_WIDGET(priv->webkit_inspector_view),
								500, 300);
	gtk_expander_set_expanded(priv->expander, TRUE);

	return TRUE;
}

gboolean remote_control_webkit_window_load(RemoteControlWebkitWindow *self,
		const gchar *uri)
{
	RemoteControlWebkitWindowPrivate *priv;

	priv = REMOTE_CONTROL_WEBKIT_WINDOW_GET_PRIVATE(self);
	webkit_web_view_load_uri(priv->webkit, uri);

	if (priv->inspector) {
		priv->webkit_inspector =
			webkit_web_view_get_inspector (WEBKIT_WEB_VIEW(priv->webkit));

		g_signal_connect (G_OBJECT (priv->webkit_inspector),
						  "inspect-web-view",
						  G_CALLBACK(remote_control_webkit_inspector_create),
						  self);
		g_signal_connect (G_OBJECT (priv->webkit_inspector),
						  "show-window",
						  G_CALLBACK(remote_control_webkit_show_inpector),
						  self);
	}

	if (priv->uri)
		g_object_unref(priv->uri);

	priv->uri = g_uri_new(uri);

	return TRUE;
}
