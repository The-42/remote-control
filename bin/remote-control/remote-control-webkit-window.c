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

#ifdef USE_WEBKIT2
#include <webkit2/webkit2.h>
#else
#include <webkit/webkit.h>
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "remote-control-webkit-window.h"
#include "remote-control-data.h"
#include "javascript.h"
#include "utils.h"
#include "guri.h"

#if GTK_CHECK_VERSION(3, 0, 0)
static const gchar style_large[] = ".scrollbar {"\
	"-GtkRange-slider-width: 50;" \
	"-GtkRange-stepper-size: 50;" \
	"}";
#endif

G_DEFINE_TYPE(RemoteControlWebkitWindow, remote_control_webkit_window, GTK_TYPE_WINDOW);

#define REMOTE_CONTROL_WEBKIT_WINDOW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), REMOTE_CONTROL_TYPE_WEBKIT_WINDOW, RemoteControlWebkitWindowPrivate))

#define WEBKIT_RELOAD_TIMEOUT 3

struct _RemoteControlWebkitWindowPrivate {
	WebKitWebView *webkit;
	GMainLoop *loop;
	GURI *uri;
	gboolean hide_cursor;
	gboolean check_origin;
	gboolean inspector;
	/* used only when inspector enabled */
#ifndef USE_WEBKIT2
	WebKitWebInspector *webkit_inspector;
#endif
	WebKitWebView *webkit_inspector_view;
	GtkExpander *expander;
#if GTK_CHECK_VERSION(3, 2, 0)
	GtkWidget *vbox;
#else
	GtkVBox *vbox;
#endif

	/* Context to control remote control server */
	struct remote_control_data *rcd;
};

enum {
	PROP_0,
	PROP_LOOP,
	PROP_CONTEXT,
	PROP_CURSOR,
	PROP_INSPECTOR,
	PROP_CHECK_ORIGIN,
	PROP_RCD
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
	GdkDisplay *display = gdk_display_get_default();
	GdkCursor *cursor = gdk_cursor_new_for_display(display,
		hide ? GDK_BLANK_CURSOR : GDK_X_CURSOR);
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

	case PROP_CHECK_ORIGIN:
		g_value_set_boolean(value, priv->check_origin);
		break;

	case PROP_RCD:
		g_value_set_pointer(value, priv->rcd);
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

	case PROP_CHECK_ORIGIN:
		priv->check_origin = g_value_get_boolean(value);
		break;

	case PROP_RCD:
		priv->rcd = g_value_get_pointer(value);
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

#ifdef USE_WEBKIT2
static void webkit_on_load_changed(WebKitWebView *webkit,
	WebKitLoadEvent load_event, gpointer data)
{
	RemoteControlWebkitWindow *window = REMOTE_CONTROL_WEBKIT_WINDOW(data);
	RemoteControlWebkitWindowPrivate *priv =
	                REMOTE_CONTROL_WEBKIT_WINDOW_GET_PRIVATE(window);

	switch (load_event) {
	case WEBKIT_LOAD_COMMITTED:
	{
		struct javascript_userdata user;
		JSGlobalContextRef context;
		int err;

		user.loop =  priv->loop;
		user.rcd = priv->rcd;
		user.window = window;

		context = webkit_web_view_get_javascript_global_context(webkit);
		g_assert(context != NULL);

		err = javascript_register(context, &user);
		if (err < 0) {
			g_debug("failed to register JavaScript API: %s",
					g_strerror(-err));
		}
		break;
	}
	default:
		break;
	}
}
#else
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
		user.rcd = priv->rcd;
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

	g_object_class_install_property(object, PROP_CHECK_ORIGIN,
			g_param_spec_boolean("check-origin", "uri origin check",
				"Enable or disable uri origin check", true,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
				G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(object, PROP_RCD,
			g_param_spec_pointer("rcd", "remote control data context",
				"Provide the remote control core context to the webview.",
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
				G_PARAM_STATIC_STRINGS));
}

static void on_realize(GtkWidget *widget, gpointer user_data)
{
	gdk_window_set_type_hint(gtk_widget_get_window(widget),
			GDK_WINDOW_TYPE_HINT_DESKTOP);
	g_object_set(widget, "hide-cursor", true, NULL);
}

#ifdef USE_WEBKIT2
static gboolean webkit_decide_policy (WebKitWebView *web_view,
	WebKitPolicyDecision *decision, WebKitPolicyDecisionType type,
	gpointer user_data)
{
	RemoteControlWebkitWindow *self = REMOTE_CONTROL_WEBKIT_WINDOW(user_data);
	RemoteControlWebkitWindowPrivate *priv;

	priv = REMOTE_CONTROL_WEBKIT_WINDOW_GET_PRIVATE(self);

	if (type == WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION) {
		WebKitNavigationPolicyDecision *navigation_decision =
			WEBKIT_NAVIGATION_POLICY_DECISION (decision);
		WebKitURIRequest *request =
			webkit_navigation_policy_decision_get_request(navigation_decision);
		const gchar *s;
		GURI *uri;

		s = webkit_uri_request_get_uri(request);
		uri = g_uri_new(s);

		if (priv->uri && priv->check_origin && !g_uri_same_origin(uri, priv->uri))
			webkit_policy_decision_ignore(decision);
		else
			webkit_policy_decision_use(decision);

		g_object_unref(uri);
	}

	return TRUE;
}
#else
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
		if (priv->uri && priv->check_origin && !g_uri_same_origin(uri, priv->uri))
			webkit_web_policy_decision_ignore(decision);
		else
			webkit_web_policy_decision_use(decision);
	} else {
		webkit_web_policy_decision_use(decision);
	}

	g_object_unref(uri);
	return TRUE;
}
#endif

static gboolean remote_control_webkit_reload(gpointer user_data)
{
	RemoteControlWebkitWindow *self = REMOTE_CONTROL_WEBKIT_WINDOW(user_data);
	RemoteControlWebkitWindowPrivate *priv;

	priv = REMOTE_CONTROL_WEBKIT_WINDOW_GET_PRIVATE(self);
	webkit_web_view_load_uri(priv->webkit, g_uri_to_string(priv->uri));
	return FALSE;
}

#ifdef USE_WEBKIT2
static gboolean webkit_handle_load_failed(WebKitWebView *webkit,
	WebKitLoadEvent load_event, gchar *uri, GError *error,
	gpointer user_data)
#else
static gboolean webkit_handle_load_error(WebKitWebView *webkit,
		WebKitWebFrame *frame, const gchar *uri, GError *error,
		gpointer user_data)
#endif
{
	gboolean need_reload = FALSE;

	g_debug("%s(): %s: %d: %s (%s)", __func__,
			g_quark_to_string(error->domain),
			error->code, error->message, uri);

	if (error->domain == WEBKIT_POLICY_ERROR) {
#ifdef USE_WEBKIT2
		if (error->code == WEBKIT_POLICY_ERROR_CANNOT_SHOW_URI)
#else
		if (error->code == WEBKIT_POLICY_ERROR_CANNOT_SHOW_URL)
#endif
			need_reload = TRUE;
	}

	if (error->domain == SOUP_HTTP_ERROR)
		need_reload = TRUE;

	if (need_reload) {
		g_debug("%s(): scheduling reload of %s...", __func__, uri);
		g_timeout_add_seconds(WEBKIT_RELOAD_TIMEOUT,
				remote_control_webkit_reload, user_data);
	}

	return FALSE;
}

#ifndef USE_WEBKIT2
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
#endif

static void remote_control_webkit_construct_view(RemoteControlWebkitWindow *self)
{
	RemoteControlWebkitWindowPrivate *priv;
	GtkWindow *window = GTK_WINDOW(self);

	priv = REMOTE_CONTROL_WEBKIT_WINDOW_GET_PRIVATE(self);

	g_print("construct view\n");
#ifdef USE_WEBKIT2
	gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(priv->webkit));
#else
	if (priv->inspector) {
		GtkWidget *view;

		view = gtk_scrolled_window_new(NULL, NULL);
		gtk_container_add(GTK_CONTAINER(view), GTK_WIDGET(priv->webkit));
		gtk_widget_show(view);

		WebKitWebSettings *settings =
				webkit_web_view_get_settings (WEBKIT_WEB_VIEW(priv->webkit));
		g_object_set (G_OBJECT(settings), "enable-developer-extras", TRUE,
					  NULL);

#if GTK_CHECK_VERSION(3, 2, 0)
		priv->vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
#else
		priv->vbox = GTK_VBOX(gtk_vbox_new(false, 0));
#endif

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
#endif
}

static void remote_control_webkit_window_init(RemoteControlWebkitWindow *self)
{
	RemoteControlWebkitWindowPrivate *priv;
	GtkWindow *window = GTK_WINDOW(self);
#if GTK_CHECK_VERSION(3, 0, 0)
	GtkCssProvider *css_provider;
#endif
	GdkScreen *screen;

	gint cx;
	gint cy;

	priv = REMOTE_CONTROL_WEBKIT_WINDOW_GET_PRIVATE(self);

	g_signal_connect(G_OBJECT(self), "realize",
			(GCallback)on_realize, NULL);

	screen = gtk_window_get_screen(window);
	cx = gdk_screen_get_width(screen);
	cy = gdk_screen_get_height(screen);

#if GTK_CHECK_VERSION(3, 0, 0)
	css_provider = gtk_css_provider_new();
	gtk_css_provider_load_from_data(css_provider, style_large, -1, NULL);
	gtk_style_context_add_provider_for_screen(screen,
			GTK_STYLE_PROVIDER(css_provider),
			GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_unref(css_provider);
#endif

	gtk_widget_set_size_request(GTK_WIDGET(window), cx, cy);

#ifndef USE_WEBKIT2
	soup_session_set_proxy(webkit_get_default_session());
#endif

	priv->webkit = WEBKIT_WEB_VIEW(webkit_web_view_new());

	webkit_set_cache_model(WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER);
	/*
	 * Add a callback to listen for load-status property changes. This is
	 * used to register the JavaScript binding within the frame.
	 */
#ifdef USE_WEBKIT2
	g_signal_connect(GTK_WIDGET(priv->webkit), "load-changed",
		G_CALLBACK(webkit_on_load_changed), self);
#else
	g_signal_connect(GTK_WIDGET(priv->webkit), "notify::load-status",
		G_CALLBACK(webkit_on_notify_load_status), self);
#endif

#ifdef USE_WEBKIT2
	g_signal_connect(G_OBJECT(priv->webkit), "decide-policy",
			G_CALLBACK(webkit_decide_policy), self);
	g_signal_connect(G_OBJECT(priv->webkit), "load-failed",
			G_CALLBACK(webkit_handle_load_failed), self);
#else
	g_signal_connect(G_OBJECT(priv->webkit),
			"navigation-policy-decision-requested",
			G_CALLBACK(navigation_policy), self);
	g_signal_connect(G_OBJECT(priv->webkit), "load-error",
			G_CALLBACK(webkit_handle_load_error), self);
#endif
}

GtkWidget *remote_control_webkit_window_new(GMainLoop *loop,
	struct remote_control_data *rcd, gboolean inspector)
{
	return g_object_new(REMOTE_CONTROL_TYPE_WEBKIT_WINDOW, "loop", loop,
						"inspector", inspector,
						"rcd", rcd, NULL);
}

#ifndef USE_WEBKIT2
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
#endif

gboolean remote_control_webkit_window_load(RemoteControlWebkitWindow *self,
		const gchar *uri)
{
	RemoteControlWebkitWindowPrivate *priv;

	priv = REMOTE_CONTROL_WEBKIT_WINDOW_GET_PRIVATE(self);
	webkit_web_view_load_uri(priv->webkit, uri);

#ifndef USE_WEBKIT2
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
#endif

	if (priv->uri)
		g_object_unref(priv->uri);

	priv->uri = g_uri_new(uri);

	return TRUE;

}

gboolean remote_control_webkit_window_reload(RemoteControlWebkitWindow *self)
{
	RemoteControlWebkitWindowPrivate *priv;

	priv = REMOTE_CONTROL_WEBKIT_WINDOW_GET_PRIVATE(self);
	webkit_web_view_reload(priv->webkit);

	return TRUE;
}
