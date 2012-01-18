/*
 * Copyright (C) 2011-2012 Avionic Design GmbH
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
#include "webkit-browser-tab-label.h"
#include "utils.h"
#include "guri.h"

static const gchar style_large[] = \
	"style \"scrollbar-large\" { GtkScrollbar::slider-width = 48 }\n" \
	"class \"GtkScrollbar\" style \"scrollbar-large\"";

#define WEBKIT_BROWSER_MAX_PAGES 8

G_DEFINE_TYPE(WebKitBrowser, webkit_browser, GTK_TYPE_WINDOW);

#define WEBKIT_BROWSER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), WEBKIT_TYPE_BROWSER, WebKitBrowserPrivate))

enum {
	PROP_0,
	PROP_GEOMETRY,
	PROP_KEYBOARD,
	PROP_CONTROLS,
};

struct _WebKitBrowserPrivate {
	GtkNotebook *notebook;
	SoupCookieJar *cookie;
	SoupLogger *logger;
	GtkToolbar *toolbar;
	GtkEntry *entry;
	GtkWidget *spinner;
	GtkToggleToolButton *toggle;
	GtkWidget *osk;
	gchar *geometry;
	gboolean keyboard;
	gboolean controls;
};

static void webkit_browser_get_property(GObject *object, guint prop_id,
		GValue *value, GParamSpec *pspec)
{
	WebKitBrowserPrivate *priv = WEBKIT_BROWSER_GET_PRIVATE(object);

	switch (prop_id) {
	case PROP_GEOMETRY:
		g_value_set_string(value, priv->geometry);
		break;

	case PROP_KEYBOARD:
		g_value_set_boolean(value, priv->keyboard);
		break;

	case PROP_CONTROLS:
		g_value_set_boolean(value, priv->controls);

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

	case PROP_KEYBOARD:
		priv->keyboard = g_value_get_boolean(value);
		g_object_set(priv->toggle, "active", priv->keyboard, NULL);
		gtk_widget_set_visible(priv->osk, priv->keyboard);
		break;

	case PROP_CONTROLS:
		priv->controls = g_value_get_boolean(value);
		if (priv->controls)
			gtk_widget_show(GTK_WIDGET(priv->toolbar));
		else
			gtk_widget_hide(GTK_WIDGET(priv->toolbar));
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
		WebKitBrowserTabLabel *label)
{
	WebKitLoadStatus status = webkit_web_view_get_load_status(webkit);

	switch (status) {
	case WEBKIT_LOAD_COMMITTED:
		webkit_browser_tab_label_set_loading(label, TRUE);
		break;

	case WEBKIT_LOAD_FINISHED:
	case WEBKIT_LOAD_FAILED:
		webkit_browser_tab_label_set_loading(label, FALSE);
		break;

	default:
		break;
	}
}

static WebKitWebView *webkit_browser_get_current_view(WebKitBrowser *browser)
{
	WebKitBrowserPrivate *priv = WEBKIT_BROWSER_GET_PRIVATE(browser);
	WebKitWebView *view;
	GtkWidget *widget;
	gint page;

	page = gtk_notebook_get_current_page(priv->notebook);
	widget = gtk_notebook_get_nth_page(priv->notebook, page);
	widget = gtk_bin_get_child(GTK_BIN(widget));
	view = WEBKIT_WEB_VIEW(widget);

	return view;
}

static void on_notify_uri(WebKitWebView *webkit, GParamSpec *pspec, WebKitBrowser *browser)
{
	WebKitBrowserPrivate *priv = WEBKIT_BROWSER_GET_PRIVATE(browser);
	WebKitWebView *view;

	view = webkit_browser_get_current_view(browser);

	if (webkit == view) {
		const gchar *uri = webkit_web_view_get_uri(webkit);
		gtk_entry_set_text(priv->entry, uri);
	}
}

static void on_back_clicked(GtkWidget *widget, gpointer data)
{
	WebKitBrowser *browser = WEBKIT_BROWSER(data);
	WebKitWebView *webkit;

	webkit = webkit_browser_get_current_view(browser);
	webkit_web_view_go_back(webkit);
}

static void on_forward_clicked(GtkWidget *widget, gpointer data)
{
	WebKitBrowser *browser = WEBKIT_BROWSER(data);
	WebKitWebView *webkit;

	webkit = webkit_browser_get_current_view(browser);
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
	WebKitBrowserPrivate *priv = WEBKIT_BROWSER_GET_PRIVATE(data);

	priv->keyboard = gtk_toggle_tool_button_get_active(priv->toggle);
	gtk_widget_set_visible(priv->osk, priv->keyboard);
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

static void on_notify_title(WebKitWebView *webkit, GParamSpec *pspec,
		WebKitBrowserTabLabel *label)
{
	const gchar *title = webkit_web_view_get_title(webkit);
	webkit_browser_tab_label_set_title(label, title);
}

static gint webkit_browser_append_tab(WebKitBrowser *browser, const gchar *title)
{
	WebKitBrowserPrivate *priv = WEBKIT_BROWSER_GET_PRIVATE(browser);
	GtkWidget *webkit;
	GtkWidget *label;
	GtkWidget *view;
	gint page;

	/* create WebKit browser */
	webkit = webkit_web_view_new();

	/* TODO: Support GtkDragView with scrollbar */
	view = gtk_scrolled_window_new(NULL, NULL);
	gtk_rc_parse_string(style_large);
	gtk_container_add(GTK_CONTAINER(view), webkit);
	gtk_widget_show_all(view);

	label = webkit_browser_tab_label_new(title);
	gtk_widget_show(label);

	page = gtk_notebook_append_page(priv->notebook, view, label);

	g_signal_connect(G_OBJECT(webkit), "notify::load-status",
			G_CALLBACK(on_notify_load_status), label);
	g_signal_connect(G_OBJECT(webkit), "notify::title",
			G_CALLBACK(on_notify_title), label);
	g_signal_connect(G_OBJECT(webkit), "notify::uri",
			G_CALLBACK(on_notify_uri), browser);

	return page;
}

static void on_add_tab_clicked(GtkWidget *widget, gpointer data)
{
	WebKitBrowserPrivate *priv = WEBKIT_BROWSER_GET_PRIVATE(data);
	WebKitBrowser *browser = WEBKIT_BROWSER(data);
	gint pages;

	pages = gtk_notebook_get_n_pages(priv->notebook);
	if (pages < WEBKIT_BROWSER_MAX_PAGES) {
		gint page = webkit_browser_append_tab(browser, NULL);
		gtk_notebook_set_current_page(priv->notebook, page);
	}
}

static void on_del_tab_clicked(GtkWidget *widget, gpointer data)
{
	WebKitBrowserPrivate *priv = WEBKIT_BROWSER_GET_PRIVATE(data);
	gint page;

	page = gtk_notebook_get_current_page(priv->notebook);
	gtk_notebook_remove_page(priv->notebook, page);
}

static void on_size_allocate(GtkNotebook *notebook, GdkRectangle *allocation,
		gpointer data)
{
	const gint max_width = 160;
	const gint min_width = 32;
	gint width = 0;
	gint pages;
	gint i;

	pages = gtk_notebook_get_n_pages(notebook);
	if (pages > 0)
		width = allocation->width / pages - 7;

	if (width < min_width)
		width = min_width;

	if (width > max_width)
		width = max_width;

	for (i = 0; i < pages; i++) {
		GtkWidget *widget = gtk_notebook_get_nth_page(notebook, i);
		GtkWidget *label = gtk_notebook_get_tab_label(notebook, widget);
		gtk_widget_set_size_request(label, width, -1);
	}
}

static void on_page_switched(GtkNotebook *notebook, GtkWidget *page,
		guint index, gpointer data)
{
	WebKitBrowserPrivate *priv = WEBKIT_BROWSER_GET_PRIVATE(data);
	WebKitWebView *webkit;
	WebKitWebFrame *frame;
	GtkWidget *widget;
	const gchar *uri;

	widget = gtk_bin_get_child(GTK_BIN(page));
	webkit = WEBKIT_WEB_VIEW(widget);
	frame = webkit_web_view_get_main_frame(webkit);
	uri = webkit_web_frame_get_uri(frame) ?: "";
	gtk_entry_set_text(priv->entry, uri);
}

static GtkWidget *webkit_browser_create_toolbar(WebKitBrowser *browser)
{
	WebKitBrowserPrivate *priv = WEBKIT_BROWSER_GET_PRIVATE(browser);
	GtkWidget *toolbar;
	GtkToolItem *item;
	GtkWidget *widget;

	/* create browser controls */
	toolbar = gtk_toolbar_new();
	priv->toolbar = GTK_TOOLBAR(toolbar);

	gtk_toolbar_set_style(priv->toolbar, GTK_TOOLBAR_ICONS);
	gtk_toolbar_set_icon_size(priv->toolbar, GTK_ICON_SIZE_DIALOG);

	item = gtk_tool_button_new_from_stock(GTK_STOCK_GO_BACK);
	g_signal_connect(G_OBJECT(item), "clicked",
			G_CALLBACK(on_back_clicked), browser);
	gtk_toolbar_insert(priv->toolbar, item, -1);
	gtk_widget_show(GTK_WIDGET(item));

	item = gtk_tool_button_new_from_stock(GTK_STOCK_GO_FORWARD);
	g_signal_connect(G_OBJECT(item), "clicked",
			G_CALLBACK(on_forward_clicked), browser);
	gtk_toolbar_insert(priv->toolbar, item, -1);
	gtk_widget_show(GTK_WIDGET(item));

	/* address entry */
	widget = gtk_entry_new();
	priv->entry = GTK_ENTRY(widget);
	g_signal_connect(G_OBJECT(widget), "activate",
			G_CALLBACK(on_uri_activate), browser);
	gtk_widget_show(widget);

	item = gtk_tool_item_new();
	gtk_tool_item_set_expand(item, TRUE);
	gtk_container_add(GTK_CONTAINER(item), widget);
	gtk_toolbar_insert(priv->toolbar, item, -1);
	gtk_widget_show(GTK_WIDGET(item));

	item = gtk_tool_button_new(NULL, NULL);
	gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(item), "go-jump");
	g_signal_connect(G_OBJECT(item), "clicked",
			G_CALLBACK(on_go_clicked), browser);
	gtk_toolbar_insert(priv->toolbar, item, -1);
	gtk_widget_show(GTK_WIDGET(item));

	priv->toggle = GTK_TOGGLE_TOOL_BUTTON(gtk_toggle_tool_button_new());
	gtk_toggle_tool_button_set_active(priv->toggle, TRUE);
	gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(priv->toggle),
			"input-keyboard");
	g_signal_connect(G_OBJECT(priv->toggle), "clicked",
			G_CALLBACK(on_keyboard_clicked), browser);
	gtk_toolbar_insert(priv->toolbar, GTK_TOOL_ITEM(priv->toggle), -1);
	gtk_widget_show(GTK_WIDGET(priv->toggle));

	item = gtk_tool_button_new(NULL, NULL);
	gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(item), "exit");
	g_signal_connect(G_OBJECT(item), "clicked",
			G_CALLBACK(on_exit_clicked), NULL);
	gtk_toolbar_insert(priv->toolbar, item, -1);
	gtk_widget_show(GTK_WIDGET(item));

	return toolbar;
}

static GtkWidget *webkit_browser_create_notebook(WebKitBrowser *browser)
{
	WebKitBrowserPrivate *priv = WEBKIT_BROWSER_GET_PRIVATE(browser);
	GtkWidget *notebook;
	GtkWidget *button;
	GtkWidget *image;
	GtkWidget *hbox;

	notebook = gtk_notebook_new();
	priv->notebook = GTK_NOTEBOOK(notebook);

	gtk_notebook_set_scrollable(priv->notebook, TRUE);

	g_signal_connect(G_OBJECT(notebook), "size-allocate",
			G_CALLBACK(on_size_allocate), browser);
	g_signal_connect(G_OBJECT(notebook), "switch-page",
			G_CALLBACK(on_page_switched), browser);

	hbox = gtk_hbox_new(FALSE, 0);

	button = gtk_button_new();
	image = gtk_image_new_from_stock(GTK_STOCK_ADD, GTK_ICON_SIZE_BUTTON);
	gtk_container_add(GTK_CONTAINER(button), image);
	g_signal_connect(G_OBJECT(button), "clicked",
			G_CALLBACK(on_add_tab_clicked), browser);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_widget_show_all(button);

	button = gtk_button_new();
	image = gtk_image_new_from_stock(GTK_STOCK_REMOVE,
			GTK_ICON_SIZE_BUTTON);
	gtk_container_add(GTK_CONTAINER(button), image);
	g_signal_connect(G_OBJECT(button), "clicked",
			G_CALLBACK(on_del_tab_clicked), browser);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_widget_show_all(button);

	gtk_notebook_set_action_widget(priv->notebook, hbox, GTK_PACK_END);
	gtk_widget_show(hbox);

	/* create initial tab */
	webkit_browser_append_tab(browser, NULL);

	return notebook;
}

static void webkit_browser_init(WebKitBrowser *browser)
{
	WebKitBrowserPrivate *priv = WEBKIT_BROWSER_GET_PRIVATE(browser);
	SoupSession *session = webkit_get_default_session();
	GtkOskLayout *layout;
	GtkWidget *notebook;
	GtkWidget *toolbar;
	GtkWidget *vbox;

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

	toolbar = webkit_browser_create_toolbar(browser);
	gtk_widget_show(toolbar);

	notebook = webkit_browser_create_notebook(browser);
	gtk_widget_show(notebook);

	/* create on-screen keyboard */
	layout = gtk_osk_layout_new(NULL);
	priv->osk = gtk_osk_new_with_layout(layout);
	gtk_widget_show(priv->osk);
	g_object_unref(layout);

#if GTK_CHECK_VERSION(3, 1, 6)
	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
#else
	vbox = gtk_vbox_new(FALSE, 0);
#endif
	gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), priv->osk, FALSE, FALSE, 0);
	gtk_widget_show(vbox);

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

	g_object_class_install_property(object, PROP_KEYBOARD,
			g_param_spec_boolean("keyboard", "On-Screen Keyboard",
				"Enable or disable the on-screen keyboard",
				TRUE,
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(object, PROP_CONTROLS,
			g_param_spec_boolean("controls", "Browser Controls",
				"Enable or disable browser controls", TRUE,
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

GtkWidget *webkit_browser_new(const gchar *geometry)
{
	return g_object_new(WEBKIT_TYPE_BROWSER, "geometry", geometry, NULL);
}

void webkit_browser_load_uri(WebKitBrowser *browser, const gchar *uri)
{
	GURI *canonical;

	if (uri && (strlen(uri) > 0)) {
		canonical = g_uri_new(uri);
		if (canonical) {
			WebKitWebView *webkit;
			gchar *final;

			webkit = webkit_browser_get_current_view(browser);

			if (!g_uri_get_scheme(canonical))
				g_uri_set_scheme(canonical, "http");

			final = g_uri_to_string(canonical);
			webkit_web_view_load_uri(webkit, final);
			g_free(final);

			g_object_unref(canonical);
		}
	}
}
