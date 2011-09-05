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

#include <string.h>
#include <gdk/gdkx.h>
#include <webkit/webkit.h>
#include <gtkosk/gtkosk.h>
#include <libsoup/soup-proxy-resolver-default.h>

#include "gtk-drag-view.h"
#include "webkit-browser.h"
#include "utils.h"
#include "guri.h"

G_DEFINE_TYPE(WebKitBrowser, webkit_browser, GTK_TYPE_WINDOW);

#define WEBKIT_BROWSER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), WEBKIT_TYPE_BROWSER, WebKitBrowserPrivate))

enum {
	PROP_0,
	PROP_GEOMETRY,
};

struct _WebKitBrowserPrivate {
	WebKitWebView *webkit;
	SoupCookieJar *cookie;
	SoupLogger *logger;
	GtkEntry *entry;
	gchar *geometry;
};

static void webkit_browser_get_property(GObject *object, guint prop_id,
		GValue *value, GParamSpec *pspec)
{
	WebKitBrowserPrivate *priv = WEBKIT_BROWSER_GET_PRIVATE(object);

	switch (prop_id) {
	case PROP_GEOMETRY:
		g_value_set_string(value, priv->geometry);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void webkit_browser_set_property(GObject *object, guint prop_id,
		const GValue *value, GParamSpec *pspec)
{
	WebKitBrowserPrivate *priv = WEBKIT_BROWSER_GET_PRIVATE(object);

	switch (prop_id) {
	case PROP_GEOMETRY:
		g_free(priv->geometry);
		priv->geometry = g_value_dup_string(value);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void webkit_browser_finalize(GObject *object)
{
	WebKitBrowserPrivate *priv = WEBKIT_BROWSER_GET_PRIVATE(object);

	g_free(priv->geometry);

	G_OBJECT_CLASS(webkit_browser_parent_class)->finalize(object);
}

static void on_notify_load_status(WebKitWebView *webkit, GParamSpec *pspec,
		WebKitBrowser *browser)
{
	WebKitBrowserPrivate *priv = WEBKIT_BROWSER_GET_PRIVATE(browser);

	if (webkit_web_view_get_load_status(webkit) == WEBKIT_LOAD_COMMITTED) {
		WebKitWebFrame *frame = webkit_web_view_get_main_frame(webkit);
		const gchar *uri = webkit_web_frame_get_uri(frame);
		if (uri)
			gtk_entry_set_text(priv->entry, uri);
	}
}

static void on_back_clicked(GtkWidget *widget, gpointer data)
{
	WebKitWebView *webkit = WEBKIT_WEB_VIEW(data);
	webkit_web_view_go_back(webkit);
}

static void on_forward_clicked(GtkWidget *widget, gpointer data)
{
	WebKitWebView *webkit = WEBKIT_WEB_VIEW(data);
	webkit_web_view_go_forward(webkit);
}

static void on_uri_activate(GtkWidget *widget, gpointer data)
{
	WebKitBrowser *browser = WEBKIT_BROWSER(data);
	GtkEntry *entry = GTK_ENTRY(widget);
	const gchar *uri = gtk_entry_get_text(entry);

	webkit_browser_load_uri(browser, uri);
}

static void on_go_clicked(GtkWidget *widget, gpointer data)
{
	WebKitBrowser *browser = WEBKIT_BROWSER(data);
	WebKitBrowserPrivate *priv;
	const gchar *uri;

	priv = WEBKIT_BROWSER_GET_PRIVATE(browser);
	uri = gtk_entry_get_text(priv->entry);
	webkit_browser_load_uri(browser, uri);
}

static void on_keyboard_clicked(GtkWidget *widget, gpointer data)
{
	GtkWidget *osk = GTK_WIDGET(data);

	if (gtk_widget_get_visible(osk))
		gtk_widget_hide(osk);
	else
		gtk_widget_show(osk);
}

static void on_exit_clicked(GtkWidget *widget, gpointer data)
{
	gtk_main_quit();
}

static void webkit_browser_realize(GtkWidget *widget)
{
	WebKitBrowserPrivate *priv = WEBKIT_BROWSER_GET_PRIVATE(widget);

	if (!priv->geometry) {
		GdkScreen *screen = gtk_widget_get_screen(widget);
		const char *wm = gdk_x11_screen_get_window_manager_name(screen);
		g_debug("Window manager: %s", wm);

		if (!wm || (strcmp(wm, "unknown") == 0)) {
			gint width = gdk_screen_get_width(screen);
			gint height = gdk_screen_get_height(screen);

			gtk_window_set_default_size(GTK_WINDOW(widget), width, height);
		} else {
			gtk_window_fullscreen(GTK_WINDOW(widget));
		}
	} else {
		gtk_window_parse_geometry(GTK_WINDOW(widget), priv->geometry);
	}

	GTK_WIDGET_CLASS(webkit_browser_parent_class)->realize(widget);
}

static void webkit_browser_init(WebKitBrowser *browser)
{
	WebKitBrowserPrivate *priv;
	SoupSession *session;
	GtkOskLayout *layout;
	GtkWidget *toolbar;
	GtkToolItem *item;
	GtkWidget *webkit;
	GtkWidget *view;
	GtkWidget *vbox;
	GtkWidget *osk;

	priv = WEBKIT_BROWSER_GET_PRIVATE(browser);

	session = webkit_get_default_session();

	/* enable cookies */
	priv->cookie = soup_cookie_jar_new();
	soup_session_add_feature(session, SOUP_SESSION_FEATURE(priv->cookie));

	/* TODO: add command-line argument to control logging */
	if (0) {
		priv->logger = soup_logger_new(SOUP_LOGGER_LOG_BODY, -1);
		soup_session_add_feature(session,
				SOUP_SESSION_FEATURE(priv->logger));
	}

	/* enable proxy support */
	soup_session_add_feature_by_type(session,
			SOUP_TYPE_PROXY_RESOLVER_DEFAULT);

	webkit = webkit_web_view_new();
	priv->webkit = WEBKIT_WEB_VIEW(webkit);
	priv->entry = GTK_ENTRY(gtk_entry_new());

	g_signal_connect(G_OBJECT(priv->webkit), "notify::load-status",
			G_CALLBACK(on_notify_load_status), browser);

	layout = gtk_osk_layout_new(NULL);
	osk = gtk_osk_new_with_layout(layout);
	g_object_unref(layout);

	toolbar = gtk_toolbar_new();

	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);

	item = gtk_tool_button_new_from_stock(GTK_STOCK_GO_BACK);
	g_signal_connect(G_OBJECT(item), "clicked",
			G_CALLBACK(on_back_clicked), priv->webkit);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);

	item = gtk_tool_button_new_from_stock(GTK_STOCK_GO_FORWARD);
	g_signal_connect(G_OBJECT(item), "clicked",
			G_CALLBACK(on_forward_clicked), priv->webkit);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);

	item = gtk_tool_item_new();
	gtk_tool_item_set_expand(item, TRUE);
	gtk_container_add(GTK_CONTAINER(item), GTK_WIDGET(priv->entry));
	g_signal_connect(G_OBJECT(priv->entry), "activate",
			G_CALLBACK(on_uri_activate), browser);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);

	item = gtk_tool_button_new_from_stock(GTK_STOCK_OK);
	g_signal_connect(G_OBJECT(item), "clicked",
			G_CALLBACK(on_go_clicked), browser);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);

	item = gtk_toggle_tool_button_new();
	gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(item), TRUE);
	gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(item), "input-keyboard");
	g_signal_connect(G_OBJECT(item), "clicked",
			G_CALLBACK(on_keyboard_clicked), osk);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);

	item = gtk_tool_button_new_from_stock(GTK_STOCK_QUIT);
	g_signal_connect(G_OBJECT(item), "clicked",
			G_CALLBACK(on_exit_clicked), NULL);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);

	view = gtk_drag_view_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(view), webkit);
	gtk_widget_show_all(view);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), view, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), osk, FALSE, FALSE, 0);
	gtk_widget_show_all(vbox);

	gtk_container_add(GTK_CONTAINER(browser), vbox);
}

static void webkit_browser_class_init(WebKitBrowserClass *class)
{
	GtkWidgetClass *widget = GTK_WIDGET_CLASS(class);
	GObjectClass *object = G_OBJECT_CLASS(class);

	g_type_class_add_private(class, sizeof(WebKitBrowserPrivate));

	object->get_property = webkit_browser_get_property;
	object->set_property = webkit_browser_set_property;
	object->finalize = webkit_browser_finalize;

	widget->realize = webkit_browser_realize;

	g_object_class_install_property(object, PROP_GEOMETRY,
			g_param_spec_string("geometry", "Geometry",
				"The window geometry.", NULL,
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

GtkWidget *webkit_browser_new(const gchar *geometry)
{
	return g_object_new(WEBKIT_TYPE_BROWSER, "geometry", geometry, NULL);
}

void webkit_browser_load_uri(WebKitBrowser *browser, const gchar *uri)
{
	WebKitBrowserPrivate *priv = WEBKIT_BROWSER_GET_PRIVATE(browser);
	GURI *canonical;

	if (uri && (strlen(uri) > 0)) {
		canonical = g_uri_new(uri);
		if (canonical) {
			gchar *final;

			if (!g_uri_get_scheme(canonical))
				g_uri_set_scheme(canonical, "http");

			final = g_uri_to_string(canonical);
			webkit_web_view_load_uri(priv->webkit, final);
			g_free(final);

			g_object_unref(canonical);
		}
	}
}
