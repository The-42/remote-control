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

#ifdef USE_WEBKIT2
#include <webkit2/webkit2.h>
#else
#include <libsoup/soup-proxy-resolver-default.h>
#include <webkit/webkit.h>
#endif

#include <gtkosk/gtkosk.h>

#ifndef USE_WEBKIT2
#include "katze-scrolled.h"
#endif
#include "webkit-browser.h"
#include "webkit-browser-tab-label.h"
#include "gtk-pdf-view.h"
#include "utils.h"
#include "guri.h"

static const gchar style_large[] = \
	"style \"scrollbar-large\" { GtkScrollbar::slider-width = 48 }\n" \
	"class \"GtkScrollbar\" style \"scrollbar-large\"";

#define WEBKIT_BROWSER_MAX_PAGES 8
#define WEBKIT_BROWSER_MIN_PAGES 1

G_DEFINE_TYPE(WebKitBrowser, webkit_browser, GTK_TYPE_WINDOW);

#define WEBKIT_BROWSER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), WEBKIT_TYPE_BROWSER, WebKitBrowserPrivate))

enum {
	PROP_0,
	PROP_GEOMETRY,
	PROP_KEYBOARD,
	PROP_CONTROLS,
	PROP_ACCEPT_LANGUAGE,
	PROP_URI,
	PROP_NOEXIT,
};

struct _WebKitBrowserPrivate {
	GtkNotebook *notebook;
	SoupCookieJar *cookie;
	SoupLogger *logger;
	GtkToolbar *toolbar;
	GtkEntry *entry;
	GtkWidget *spinner;
	GtkToolItem *back;
	GtkToolItem *forward;
	GtkToolItem *reload;
	GtkToggleToolButton *toggle;
	GtkToolItem *exit;
	GtkWidget *osk;
	gchar *geometry;
	gboolean keyboard;
	gboolean controls;
	gboolean noexit;
	gchar *uri;
	GtkWidget *addTab;
	GtkWidget *delTab;

#ifdef USE_WEBKIT2
	gchar **languages;
#endif
};

#ifdef USE_WEBKIT2
static gchar *webkit_browser_get_accept_language(WebKitBrowser *browser)
{
	WebKitBrowserPrivate *priv = WEBKIT_BROWSER_GET_PRIVATE(browser);

	return g_strjoinv(";", priv->languages);
}

static void webkit_browser_set_accept_language(WebKitBrowser *browser,
					       const gchar *language)
{
	WebKitBrowserPrivate *priv = WEBKIT_BROWSER_GET_PRIVATE(browser);
	WebKitWebContext *context = webkit_web_context_get_default();
	gchar **languages;

	languages = g_strsplit(language, ";", 0);

	webkit_web_context_set_preferred_languages(context,
			(const gchar *const *)languages);

	g_strfreev(priv->languages);
	priv->languages = languages;
}
#else
static gchar *webkit_browser_get_accept_language(WebKitBrowser *browser)
{
	SoupSession *session = webkit_get_default_session();
	gchar *language;

	g_return_val_if_fail(SOUP_IS_SESSION(session), NULL);

	g_object_get(session, SOUP_SESSION_ACCEPT_LANGUAGE, &language, NULL);

	return language;
}

static void webkit_browser_set_accept_language(WebKitBrowser *browser,
					       const gchar *language)
{
	SoupSession *session = webkit_get_default_session();

	g_return_if_fail(SOUP_IS_SESSION(session));

	g_object_set(session, SOUP_SESSION_ACCEPT_LANGUAGE, language, NULL);
}
#endif

static void webkit_browser_get_property(GObject *object, guint prop_id,
		GValue *value, GParamSpec *pspec)
{
	WebKitBrowserPrivate *priv = WEBKIT_BROWSER_GET_PRIVATE(object);
	WebKitBrowser *browser = WEBKIT_BROWSER(object);
	gchar *language;

	switch (prop_id) {
	case PROP_GEOMETRY:
		g_value_set_string(value, priv->geometry);
		break;

	case PROP_KEYBOARD:
		g_value_set_boolean(value, priv->keyboard);
		break;

	case PROP_CONTROLS:
		g_value_set_boolean(value, priv->controls);
		break;

	case PROP_ACCEPT_LANGUAGE:
		language = webkit_browser_get_accept_language(browser);
		g_value_take_string(value, language);
		break;

	case PROP_URI:
		g_value_set_string(value, priv->uri);
		break;

	case PROP_NOEXIT:
		g_value_set_boolean(value, priv->noexit);
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
	WebKitBrowser *browser = WEBKIT_BROWSER(object);

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
		if (priv->controls) {
			gtk_widget_show(GTK_WIDGET(priv->toolbar));
			gtk_notebook_set_show_tabs(priv->notebook, TRUE);
		} else {
			gtk_widget_hide(GTK_WIDGET(priv->toolbar));
			gtk_notebook_set_show_tabs(priv->notebook, FALSE);
		}
		break;

	case PROP_ACCEPT_LANGUAGE:
		webkit_browser_set_accept_language(browser,
						   g_value_get_string(value));
		break;

	case PROP_URI:
		break;

	case PROP_NOEXIT:
		priv->noexit = g_value_get_boolean(value);
		if (priv->noexit)
			gtk_widget_hide(GTK_WIDGET(priv->exit));
		else
			gtk_widget_show(GTK_WIDGET(priv->exit));
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

#ifdef USE_WEBKIT2
static void on_notify_progress(WebKitWebView *webkit, GParamSpec *pspec,
		WebKitBrowser *browser)
{
	WebKitBrowserPrivate *priv = WEBKIT_BROWSER_GET_PRIVATE(browser);
	gdouble progress =
		webkit_web_view_get_estimated_load_progress(webkit);

	if (progress == 1.0)
		progress = 0.0;
	gtk_entry_set_progress_fraction(priv->entry, progress);
}
#else
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

static void on_notify_load_status_global(WebKitWebView *webkit,
		GParamSpec *pspec, WebKitBrowser *browser)
{
	WebKitLoadStatus status = webkit_web_view_get_load_status(webkit);
	WebKitBrowserPrivate *priv = WEBKIT_BROWSER_GET_PRIVATE(browser);

	switch (status) {
	case WEBKIT_LOAD_COMMITTED:
		gtk_toggle_tool_button_set_active(priv->toggle, false);
		break;

	case WEBKIT_LOAD_FINISHED:
	case WEBKIT_LOAD_FAILED:
		gtk_entry_set_progress_fraction(priv->entry, 0.0);
		break;

	default:
		break;
	}
}

static void on_notify_progress(WebKitWebView *webkit, GParamSpec *pspec,
		WebKitBrowser *browser)
{
	WebKitBrowserPrivate *priv = WEBKIT_BROWSER_GET_PRIVATE(browser);
	WebKitLoadStatus status = webkit_web_view_get_load_status(webkit);
	gdouble progress = webkit_web_view_get_progress(webkit);

	switch (status) {
	case WEBKIT_LOAD_FINISHED:
	case WEBKIT_LOAD_FAILED:
		progress = 0.0;
		break;
	default:
		break;
	}
	gtk_entry_set_progress_fraction(priv->entry, progress);
}
#endif

static WebKitWebView *webkit_browser_get_current_view(WebKitBrowser *browser)
{
	WebKitBrowserPrivate *priv = WEBKIT_BROWSER_GET_PRIVATE(browser);
	WebKitWebView *view;
	GtkWidget *widget;
	gint page;

	page = gtk_notebook_get_current_page(priv->notebook);
	widget = gtk_notebook_get_nth_page(priv->notebook, page);
#ifdef USE_WEBKIT2
	view = WEBKIT_WEB_VIEW(widget);
#else
	widget = gtk_bin_get_child(GTK_BIN(gtk_bin_get_child(GTK_BIN(widget))));
	view = WEBKIT_WEB_VIEW(widget);
#endif

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

		g_free(priv->uri);
		priv->uri = g_strdup(uri);
		g_object_notify(G_OBJECT(browser), "uri");
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
	WebKitWebView *webkit;
	const gchar *uri;

	webkit = webkit_browser_get_current_view(browser);
	uri = gtk_entry_get_text(entry);

	webkit_browser_load_uri(browser, uri);
	gtk_widget_grab_focus(GTK_WIDGET(webkit));
}

static gboolean on_uri_focus(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	WebKitBrowserPrivate *priv = WEBKIT_BROWSER_GET_PRIVATE(data);
	GtkEditable *editable = GTK_EDITABLE(widget);

	gtk_toggle_tool_button_set_active(priv->toggle, TRUE);

	if (!gtk_widget_has_focus(widget)) {
		gtk_editable_select_region(editable, 0, -1);
		gtk_widget_grab_focus(widget);
		return TRUE;
	}

	return FALSE;
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
	GtkWidget *window = GTK_WIDGET(data);

	gtk_widget_destroy(window);
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

static void on_notify_title(GObject *object, GParamSpec *pspec,
		WebKitBrowserTabLabel *label)
{
	gchar *title = NULL;

	g_object_get(object, "title", &title, NULL);
	webkit_browser_tab_label_set_title(label, title);

	g_free(title);
}

static void on_notify_loading(GObject *object, GParamSpec *pspec,
		WebKitBrowserTabLabel *label)
{
	gboolean loading;

	g_object_get(object, "loading", &loading, NULL);

	webkit_browser_tab_label_set_loading(label, loading);
}

static gint webkit_browser_append_tab(WebKitBrowser *browser, const gchar *title, gboolean hidden);
static gint webkit_browser_append_page_with_pdf(WebKitBrowser *browser, WebKitDownload *download);

static void webkit_browser_update_tab_controls(WebKitBrowserPrivate *priv)
{
	gint pages = gtk_notebook_get_n_pages(priv->notebook);

	if (pages == WEBKIT_BROWSER_MAX_PAGES) {
		gtk_widget_set_sensitive(priv->addTab, FALSE);
		gtk_widget_set_sensitive(priv->delTab, FALSE);
	} else if (pages == WEBKIT_BROWSER_MIN_PAGES) {
		gtk_widget_set_sensitive(priv->addTab, TRUE);
		gtk_widget_set_sensitive(priv->delTab, FALSE);
	} else {
		gtk_widget_set_sensitive(priv->addTab, TRUE);
		gtk_widget_set_sensitive(priv->delTab, TRUE);
	}
}

#ifdef USE_WEBKIT2
static gboolean on_download_created_destination(WebKitDownload *download,
	gchar *destination, gpointer data)
{
	WebKitBrowserPrivate *priv = WEBKIT_BROWSER_GET_PRIVATE(data);
	WebKitBrowser *browser = (WebKitBrowser *)data;
	gint page;

	g_debug("download started");
	page = webkit_browser_append_page_with_pdf(browser, download);
	if (page >= 0)
		gtk_notebook_set_current_page(priv->notebook, page);

	return TRUE;
}

static void webkit_download_started(WebKitWebContext *context,
	WebKitDownload *download, gpointer user_data)
{
	const gchar *template = "download-XXXXXX";
	gchar *filename = NULL;
	GError *error = NULL;
	gchar *uri;
	gint fd;

	fd = g_file_open_tmp(template, &filename, &error);
	if (fd < 0) {
		g_debug("g_file_open_tmp(): %s", error->message);
		g_error_free(error);
		return;
	}

	close(fd);

	g_print("Webkit download started! Loading to: %s\n", filename);

	uri = g_strdup_printf("file://%s", filename);
	g_debug("downloading to %s", filename);

	g_signal_connect(G_OBJECT(download), "created-destination",
			G_CALLBACK(on_download_created_destination), user_data);
	webkit_download_set_destination(download, uri);

	unlink(filename);
	g_free(filename);
	g_free(uri);
	return;
}
#else
static void on_download_status(WebKitDownload *download, GParamSpec *pspec, gpointer data)
{
	WebKitDownloadStatus status = webkit_download_get_status(download);
	WebKitBrowserPrivate *priv = WEBKIT_BROWSER_GET_PRIVATE(data);
	WebKitBrowser *browser = data;
	GtkWidget *widget;
	GtkWidget *label;
	gint page;

	switch (status) {
	case WEBKIT_DOWNLOAD_STATUS_ERROR:
		g_debug("download failed");
		break;

	case WEBKIT_DOWNLOAD_STATUS_CREATED:
		g_debug("download created");
		break;

	case WEBKIT_DOWNLOAD_STATUS_STARTED:
		g_debug("download started");
		page = webkit_browser_append_page_with_pdf(browser, download);
		if (page >= 0)
			gtk_notebook_set_current_page(priv->notebook, page);
		break;

	case WEBKIT_DOWNLOAD_STATUS_CANCELLED:
		g_debug("download cancelled");
		page = gtk_notebook_get_current_page(priv->notebook);
		widget = gtk_notebook_get_nth_page(priv->notebook, page);
		label = gtk_notebook_get_tab_label(priv->notebook, widget);
		webkit_browser_tab_label_set_loading(WEBKIT_BROWSER_TAB_LABEL(label), FALSE);
		break;

	case WEBKIT_DOWNLOAD_STATUS_FINISHED:
		g_debug("download finished");
		page = gtk_notebook_get_current_page(priv->notebook);
		widget = gtk_notebook_get_nth_page(priv->notebook, page);
		label = gtk_notebook_get_tab_label(priv->notebook, widget);
		webkit_browser_tab_label_set_loading(WEBKIT_BROWSER_TAB_LABEL(label), FALSE);
		break;
	}
}

static gboolean on_download_requested(WebKitWebView *webkit,
		WebKitDownload *download, gpointer data)
{
	const gchar *template = "download-XXXXXX";
	gchar *filename = NULL;
	GError *error = NULL;
	gchar *uri;
	gint fd;

	fd = g_file_open_tmp(template, &filename, &error);
	if (fd < 0) {
		g_debug("g_file_open_tmp(): %s", error->message);
		g_error_free(error);
		return FALSE;
	}

	close(fd);

	uri = g_strdup_printf("file://%s", filename);
	g_debug("downloading to %s", filename);

	webkit_download_set_destination_uri(download, uri);
	g_signal_connect(download, "notify::status", G_CALLBACK(on_download_status), data);

	unlink(filename);
	g_free(filename);
	g_free(uri);
	return TRUE;
}
#endif

static gboolean webkit_browser_can_open_tab(WebKitBrowser *browser)
{
	WebKitBrowserPrivate *priv = WEBKIT_BROWSER_GET_PRIVATE(browser);
	gint pages = gtk_notebook_get_n_pages(priv->notebook);

	if (gtk_notebook_get_show_tabs(priv->notebook)) {
		if (pages < WEBKIT_BROWSER_MAX_PAGES)
			return TRUE;
	} else {
		/* Allow only one tab when tabbar is hidden */
		if (pages < 1)
			return TRUE;
	}

	return FALSE;
}

#ifndef USE_WEBKIT2
static gboolean on_mime_type_requested(WebKitWebView *webkit, WebKitWebFrame *frame,
		WebKitNetworkRequest *request, const gchar *mimetype,
		WebKitWebPolicyDecision *decision, gpointer data)
{
	if (g_str_equal(mimetype, "application/pdf")) {
		webkit_web_policy_decision_download(decision);
		return TRUE;
	}

	return FALSE;
}

static gboolean on_new_window_requested(WebKitWebView *webkit,
		WebKitWebFrame *frame, WebKitNetworkRequest *request,
		WebKitWebNavigationAction *action,
		WebKitWebPolicyDecision *decision, gpointer user_data)
{
	WebKitBrowser *browser = WEBKIT_BROWSER(user_data);

	if (!webkit_browser_can_open_tab(browser))
		webkit_web_policy_decision_ignore(decision);
	else
		webkit_web_policy_decision_use(decision);

	return TRUE;
}
#else
static gboolean webkit_decide_policy (WebKitWebView *web_view,
	WebKitPolicyDecision *decision, WebKitPolicyDecisionType type,
	gpointer user_data)
{
	WebKitBrowser *browser = WEBKIT_BROWSER(user_data);

	if (type == WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION) {
		if (!webkit_browser_can_open_tab(browser))
			webkit_policy_decision_ignore(decision);
		else
			webkit_policy_decision_use(decision);
	} else if (type == WEBKIT_POLICY_DECISION_TYPE_RESPONSE) {
		WebKitResponsePolicyDecision *response_policy =
			WEBKIT_RESPONSE_POLICY_DECISION(decision);
		WebKitURIResponse *response =
			webkit_response_policy_decision_get_response(response_policy);
		const gchar *mimetype =	webkit_uri_response_get_mime_type(response);

		if (g_str_equal(mimetype, "application/pdf")) {
			webkit_policy_decision_download(decision);
		} else if (g_ascii_strncasecmp(mimetype, "application/", 12)
				== 0) {
			webkit_policy_decision_ignore(decision);
		}
	}

	return TRUE;
}
#endif

static gboolean on_web_view_ready(WebKitWebView *webkit, gpointer user_data)
{
	gtk_widget_show_all(GTK_WIDGET(user_data));

	return FALSE;
}

static WebKitWebView *on_create_web_view(WebKitWebView *web_view,
	gpointer user_data)
{
	WebKitBrowserPrivate *priv = WEBKIT_BROWSER_GET_PRIVATE(user_data);
	WebKitBrowser *browser = WEBKIT_BROWSER(user_data);
	GtkWidget *view;
	GtkWidget *new;
	gint page;

	page = webkit_browser_append_tab(browser, NULL, TRUE);
	gtk_notebook_set_current_page(priv->notebook, page);

	view = gtk_notebook_get_nth_page(priv->notebook, page);
	new = gtk_bin_get_child(GTK_BIN(view));

	webkit_browser_update_tab_controls(priv);

	return WEBKIT_WEB_VIEW(new);
}

static void webkit_browser_set_user_agent(WebKitWebView *webkit)
{
// FIXME: As of v1.8.1 webkit2 lacks a user agent.
#ifndef USE_WEBKIT2
	WebKitWebSettings *settings;
	gchar *user_agent;
	gchar *processor;

	settings = webkit_web_settings_new();
	user_agent = g_strdup(webkit_web_settings_get_user_agent(settings));

	/* if armv7l is set as processor, we simply remove it,
	 * as some sites load mobile page versions for any arm
	 * device */
	processor = g_strstr_len(user_agent, -1, " armv7l");
	if(processor) {
		gchar *processor_end = processor + 7;
		memmove(processor, processor_end, strlen(processor_end)+1);
	}

	g_object_set(G_OBJECT(settings), "user-agent", user_agent, NULL);
	webkit_web_view_set_settings (WEBKIT_WEB_VIEW(webkit), settings);
	g_free(user_agent);
#else
	#warning "User Agent setting is not implemented in Webkit2"
#endif
}

static gint webkit_browser_append_tab(WebKitBrowser *browser, const gchar *title, gboolean hidden)
{
	WebKitBrowserPrivate *priv = WEBKIT_BROWSER_GET_PRIVATE(browser);
#ifdef USE_WEBKIT2
	WebKitWebContext *context = webkit_web_context_get_default();
#endif
	GtkWidget *webkit;
	GtkWidget *label;
#ifndef USE_WEBKIT2
	GtkWidget *view;
#endif
	gint page;

	if (!webkit_browser_can_open_tab(browser))
		return -1;

	/* create WebKit browser */
	webkit = webkit_web_view_new();
	webkit_browser_set_user_agent(WEBKIT_WEB_VIEW(webkit));
	gtk_widget_set_double_buffered(webkit, TRUE);

	/* TODO: Support GtkDragView with scrollbar */
#ifndef USE_WEBKIT2
	view = katze_scrolled_new(NULL, NULL);
	g_object_set(G_OBJECT(view), "drag-scrolling", TRUE, NULL);
	gtk_rc_parse_string(style_large);
	gtk_container_add(GTK_CONTAINER(view), webkit);
	gtk_widget_show(view);
#endif
	if (!hidden)
		gtk_widget_show(webkit);

	label = webkit_browser_tab_label_new(title);
	gtk_widget_show(label);

#ifdef USE_WEBKIT2
	page = gtk_notebook_append_page(priv->notebook, webkit, label);

	g_signal_connect(G_OBJECT(webkit), "decide-policy",
			G_CALLBACK(webkit_decide_policy), browser);
	g_signal_connect(G_OBJECT(context), "download-started",
			G_CALLBACK(webkit_download_started), browser);
	g_signal_connect(G_OBJECT(webkit), "notify::estimated-load-progress",
			G_CALLBACK(on_notify_progress), browser);
#else
	page = gtk_notebook_append_page(priv->notebook, view, label);

	g_signal_connect(G_OBJECT(webkit), "mime-type-policy-decision-requested",
			G_CALLBACK(on_mime_type_requested), NULL);
	g_signal_connect(G_OBJECT(webkit), "download-requested",
			G_CALLBACK(on_download_requested), browser);
	g_signal_connect(G_OBJECT(webkit), "new-window-policy-decision-requested",
			G_CALLBACK(on_new_window_requested), browser);
	g_signal_connect(G_OBJECT(webkit), "notify::load-status",
			G_CALLBACK(on_notify_load_status), label);
	g_signal_connect(G_OBJECT(webkit), "notify::load-status",
			G_CALLBACK(on_notify_load_status_global), browser);
	g_signal_connect(G_OBJECT(webkit), "notify::progress",
			G_CALLBACK(on_notify_progress), browser);
#endif
	g_signal_connect(G_OBJECT(webkit), "notify::create",
			G_CALLBACK(on_create_web_view), browser);

	if (hidden)
		g_signal_connect(G_OBJECT(webkit), "web-view-ready",
				G_CALLBACK(on_web_view_ready), webkit);

	g_signal_connect(G_OBJECT(webkit), "notify::title",
			G_CALLBACK(on_notify_title), label);
	g_signal_connect(G_OBJECT(webkit), "notify::uri",
			G_CALLBACK(on_notify_uri), browser);

	return page;
}

static gint webkit_browser_append_page_with_pdf(WebKitBrowser *browser, WebKitDownload *download)
{
	WebKitBrowserPrivate *priv = WEBKIT_BROWSER_GET_PRIVATE(browser);
	const gchar *title = "Loading PDF...";
	GtkWidget *label;
	GtkWidget *view;
	gint page;

	/*
	 * In kiosk mode, only a single tab can be open at a time because
	 * there are no controls available to navigate or close tabs. To
	 * allow viewing PDF files we need to remove the existing tab and
	 * open a new one with the PDF viewer instead.
	 */
	if (!gtk_notebook_get_show_tabs(priv->notebook)) {
		page = gtk_notebook_get_current_page(priv->notebook);
		gtk_notebook_remove_page(priv->notebook, page);
	}

	if (!webkit_browser_can_open_tab(browser))
		return -1;

	view = gtk_pdf_view_new(download);
	gtk_widget_show(view);

	label = webkit_browser_tab_label_new(title);
	gtk_widget_show(label);

	webkit_browser_tab_label_set_loading(WEBKIT_BROWSER_TAB_LABEL(label), TRUE);
	g_signal_connect(G_OBJECT(view), "notify::title",
			G_CALLBACK(on_notify_title), label);
	g_signal_connect(G_OBJECT(view), "notify::loading",
			G_CALLBACK(on_notify_loading), label);

	page = gtk_notebook_append_page(priv->notebook, view, label);
	webkit_browser_update_tab_controls(priv);
	return page;
}

static void on_add_tab_clicked(GtkWidget *widget, gpointer data)
{
	WebKitBrowserPrivate *priv = WEBKIT_BROWSER_GET_PRIVATE(data);
	WebKitBrowser *browser = WEBKIT_BROWSER(data);
	gint page;

	page = webkit_browser_append_tab(browser, NULL, FALSE);
	if (page >= 0) {
		gtk_notebook_set_current_page(priv->notebook, page);
		gtk_widget_grab_focus(GTK_WIDGET(priv->entry));
		gtk_toggle_tool_button_set_active(priv->toggle, TRUE);

		webkit_browser_update_tab_controls(priv);
	}
}

static void on_del_tab_clicked(GtkWidget *widget, gpointer data)
{
	WebKitBrowserPrivate *priv = WEBKIT_BROWSER_GET_PRIVATE(data);
	gint pages = gtk_notebook_get_n_pages(priv->notebook);
	gint page;

	if (pages > WEBKIT_BROWSER_MIN_PAGES) {
		page = gtk_notebook_get_current_page(priv->notebook);
		gtk_notebook_remove_page(priv->notebook, page);

		webkit_browser_update_tab_controls(priv);
	}
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
#ifndef USE_WEBKIT2
	WebKitWebFrame *frame;
#endif
	GtkWidget *widget;
	const gchar *uri;

#ifdef USE_WEBKIT2
	if (WEBKIT_IS_WEB_VIEW(page)) {
		/* the notebook page contains a WebKitWebView */
		widget = gtk_bin_get_child(GTK_BIN(gtk_bin_get_child(GTK_BIN(page))));
		webkit = WEBKIT_WEB_VIEW(widget);
#else
	if (GTK_IS_BIN(page)) {
		/* the notebook page contains a WebKitWebView */
		widget = gtk_bin_get_child(GTK_BIN(page));
		webkit = WEBKIT_WEB_VIEW(widget);
		frame = webkit_web_view_get_main_frame(webkit);
#endif

		gtk_widget_set_sensitive(GTK_WIDGET(priv->back), TRUE);
		gtk_widget_set_sensitive(GTK_WIDGET(priv->forward), TRUE);
		gtk_widget_set_sensitive(GTK_WIDGET(priv->entry), TRUE);
		gtk_widget_set_sensitive(GTK_WIDGET(priv->reload), TRUE);

#ifdef USE_WEBKIT2
		uri = webkit_web_view_get_uri(webkit) ?: "";
#else
		uri = webkit_web_frame_get_uri(frame) ?: "";
#endif
		gtk_entry_set_text(priv->entry, uri);
	} else {
		/* the notebook page contains a GtkPdfView */
		gtk_widget_set_sensitive(GTK_WIDGET(priv->back), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(priv->forward), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(priv->entry), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(priv->reload), FALSE);
		/* TODO: set a meaningful URI for the PDF */
		gtk_entry_set_text(priv->entry, "");
	}
}

static GtkWidget *webkit_browser_create_toolbar(WebKitBrowser *browser)
{
	WebKitBrowserPrivate *priv = WEBKIT_BROWSER_GET_PRIVATE(browser);
	PangoFontDescription *font_desc;
	GtkWidget *toolbar;
	GtkToolItem *item;
	GtkWidget *widget;

	/* create browser controls */
	toolbar = gtk_toolbar_new();
	priv->toolbar = GTK_TOOLBAR(toolbar);

	gtk_toolbar_set_style(priv->toolbar, GTK_TOOLBAR_ICONS);
	gtk_toolbar_set_icon_size(priv->toolbar, GTK_ICON_SIZE_DIALOG);

	priv->back = gtk_tool_button_new_from_stock(GTK_STOCK_GO_BACK);
	g_signal_connect(G_OBJECT(priv->back), "clicked",
			G_CALLBACK(on_back_clicked), browser);
	gtk_toolbar_insert(priv->toolbar, priv->back, -1);
	gtk_widget_show(GTK_WIDGET(priv->back));

	priv->forward = gtk_tool_button_new_from_stock(GTK_STOCK_GO_FORWARD);
	g_signal_connect(G_OBJECT(priv->forward), "clicked",
			G_CALLBACK(on_forward_clicked), browser);
	gtk_toolbar_insert(priv->toolbar, priv->forward, -1);
	gtk_widget_show(GTK_WIDGET(priv->forward));

	/* address entry */
	widget = gtk_entry_new();
	font_desc = pango_font_description_new();
	pango_font_description_set_size(font_desc, 16 * PANGO_SCALE);
	gtk_widget_modify_font(widget, font_desc);
	pango_font_description_free(font_desc);

	priv->entry = GTK_ENTRY(widget);
	g_signal_connect(G_OBJECT(widget), "activate",
			G_CALLBACK(on_uri_activate), browser);
	g_signal_connect(G_OBJECT(widget), "button-press-event",
			G_CALLBACK(on_uri_focus), browser);
	gtk_widget_show(widget);

	item = gtk_tool_item_new();
	gtk_tool_item_set_expand(item, TRUE);
	gtk_container_add(GTK_CONTAINER(item), widget);
	gtk_toolbar_insert(priv->toolbar, item, -1);
	gtk_widget_show(GTK_WIDGET(item));

	priv->reload = gtk_tool_button_new(NULL, NULL);
	gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(priv->reload),
			"go-jump");
	g_signal_connect(G_OBJECT(priv->reload), "clicked",
			G_CALLBACK(on_go_clicked), browser);
	gtk_toolbar_insert(priv->toolbar, priv->reload, -1);
	gtk_widget_show(GTK_WIDGET(priv->reload));

	priv->toggle = GTK_TOGGLE_TOOL_BUTTON(gtk_toggle_tool_button_new());
	gtk_toggle_tool_button_set_active(priv->toggle, TRUE);
	gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(priv->toggle),
			"input-keyboard");
	g_signal_connect(G_OBJECT(priv->toggle), "toggled",
			G_CALLBACK(on_keyboard_clicked), browser);
	gtk_toolbar_insert(priv->toolbar, GTK_TOOL_ITEM(priv->toggle), -1);
	gtk_widget_show(GTK_WIDGET(priv->toggle));

	priv->exit = gtk_tool_button_new(NULL, NULL);
	gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(priv->exit), "exit");
	g_signal_connect(G_OBJECT(priv->exit), "clicked",
			G_CALLBACK(on_exit_clicked), browser);
	gtk_toolbar_insert(priv->toolbar, priv->exit, -1);
	gtk_widget_show(GTK_WIDGET(priv->exit));

	return toolbar;
}

static GtkWidget *webkit_browser_create_notebook(WebKitBrowser *browser)
{
	WebKitBrowserPrivate *priv = WEBKIT_BROWSER_GET_PRIVATE(browser);
	GtkWidget *notebook;
	GtkWidget *image;
	GtkWidget *hbox;

	notebook = gtk_notebook_new();
	priv->notebook = GTK_NOTEBOOK(notebook);

	gtk_notebook_set_scrollable(priv->notebook, TRUE);

	g_signal_connect(G_OBJECT(notebook), "size-allocate",
			G_CALLBACK(on_size_allocate), browser);
	g_signal_connect(G_OBJECT(notebook), "switch-page",
			G_CALLBACK(on_page_switched), browser);

#if GTK_CHECK_VERSION(3, 0, 0)
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
#else
	hbox = gtk_hbox_new(FALSE, 0);
#endif

	priv->addTab = gtk_button_new();
	image = gtk_image_new_from_stock(GTK_STOCK_ADD, GTK_ICON_SIZE_BUTTON);
	gtk_container_add(GTK_CONTAINER(priv->addTab), image);
	g_signal_connect(G_OBJECT(priv->addTab), "clicked",
			G_CALLBACK(on_add_tab_clicked), browser);
	gtk_box_pack_start(GTK_BOX(hbox), priv->addTab, FALSE, FALSE, 0);
	gtk_widget_show_all(priv->addTab);

	priv->delTab = gtk_button_new();
	image = gtk_image_new_from_stock(GTK_STOCK_REMOVE,
			GTK_ICON_SIZE_BUTTON);
	gtk_container_add(GTK_CONTAINER(priv->delTab), image);
	g_signal_connect(G_OBJECT(priv->delTab), "clicked",
			G_CALLBACK(on_del_tab_clicked), browser);
	gtk_box_pack_start(GTK_BOX(hbox), priv->delTab, FALSE, FALSE, 10);
	gtk_widget_show_all(priv->delTab);

	gtk_notebook_set_action_widget(priv->notebook, hbox, GTK_PACK_END);
	gtk_widget_show(hbox);

	/* create initial tab */
	webkit_browser_append_tab(browser, NULL, FALSE);

	return notebook;
}

static void webkit_browser_init(WebKitBrowser *browser)
{
	WebKitBrowserPrivate *priv = WEBKIT_BROWSER_GET_PRIVATE(browser);
#ifndef USE_WEBKIT2
	SoupSession *session = webkit_get_default_session();
#endif
	GtkOskLayout *layout;
	GtkWidget *notebook;
	GtkWidget *toolbar;
	GtkWidget *vbox;

	/* enable cookies */
#ifndef USE_WEBKIT2
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
#else
	#warning "Proxy support missing in webkit2"
	#warning "Cookie support missing in webkit2"
#endif

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

	webkit_browser_update_tab_controls(priv);
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

	g_object_class_install_property(object, PROP_ACCEPT_LANGUAGE,
			g_param_spec_string("accept-language",
				"Accept-Language string",
				"Accept-Language string",
				NULL,
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(object, PROP_URI,
			g_param_spec_string("uri", "URI", "URI", NULL,
				G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(object, PROP_NOEXIT,
			g_param_spec_boolean("no-exit", "No Exit",
				"Disable exit button", FALSE,
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

gchar *webkit_browser_get_uri(WebKitBrowser *browser)
{
	WebKitBrowserPrivate *priv = WEBKIT_BROWSER_GET_PRIVATE(browser);

	return g_strdup(priv->uri);
}
