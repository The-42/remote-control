/*
 * Copyright (C) 2011-2013 Avionic Design GmbH
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

#if !GTK_CHECK_VERSION(3, 0, 0)
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
	PROP_USER_AGENT,
	PROP_MAX_PAGES,
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
	GtkWidget *add_tab;
	GtkWidget *del_tab;
	guint max_pages;
	gchar *user_agent;

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

static void webkit_browser_update_tab_controls(WebKitBrowserPrivate *priv)
{
	gint pages = gtk_notebook_get_n_pages(priv->notebook);

	if (pages == priv->max_pages)
		gtk_widget_set_sensitive(priv->add_tab, FALSE);
	else
		gtk_widget_set_sensitive(priv->add_tab, TRUE);

	if (pages == WEBKIT_BROWSER_MIN_PAGES)
		gtk_widget_set_sensitive(priv->del_tab, FALSE);
	else
		gtk_widget_set_sensitive(priv->del_tab, TRUE);
}

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

	case PROP_USER_AGENT:
		g_value_set_string(value, priv->user_agent);
		break;

	case PROP_NOEXIT:
		g_value_set_boolean(value, priv->noexit);
		break;

	case PROP_MAX_PAGES:
		g_value_set_uint(value, priv->max_pages);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void webkit_browser_set_user_agent(WebKitWebView *webkit, const gchar *str);
static WebKitWebView *webkit_browser_get_current_view(WebKitBrowser *browser);

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

	case PROP_MAX_PAGES:
		priv->max_pages = g_value_get_uint(value);
		webkit_browser_update_tab_controls(priv);
		break;

	case PROP_USER_AGENT:
		if (priv->user_agent)
			g_free(priv->user_agent);
		priv->user_agent = g_value_dup_string(value);
		/* update the current page */
		webkit_browser_set_user_agent(webkit_browser_get_current_view(browser),
			priv->user_agent);
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
	g_free(priv->user_agent);

	G_OBJECT_CLASS(webkit_browser_parent_class)->finalize(object);
}

static gboolean webkit_web_view_is_loading(WebKitWebView *webkit)
{
	WebKitLoadStatus status = webkit_web_view_get_load_status(webkit);

	switch (status) {
	case WEBKIT_LOAD_COMMITTED:
	case WEBKIT_LOAD_FIRST_VISUALLY_NON_EMPTY_LAYOUT:
		return TRUE;

	case WEBKIT_LOAD_FINISHED:
	case WEBKIT_LOAD_FAILED:
	default:
		return FALSE;
	}

	return FALSE;
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
	gboolean loading = webkit_web_view_is_loading(webkit);

	webkit_browser_tab_label_set_loading(label, loading);
}

static void on_notify_load_status_global(WebKitWebView *webkit,
		GParamSpec *pspec, WebKitBrowser *browser)
{
	WebKitBrowserPrivate *priv = WEBKIT_BROWSER_GET_PRIVATE(browser);

	if (webkit_web_view_is_loading(webkit)) {
		gtk_toggle_tool_button_set_active(priv->toggle, false);
		gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(priv->reload),
					      "stop");
	} else {
		gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(priv->reload),
					      "go-jump");
		gtk_entry_set_progress_fraction(priv->entry, 0.0);
	}
}

static void on_notify_progress(WebKitWebView *webkit, GParamSpec *pspec,
		WebKitBrowser *browser)
{
	WebKitBrowserPrivate *priv = WEBKIT_BROWSER_GET_PRIVATE(browser);
	gdouble progress = 0.0;

	if (webkit_web_view_is_loading(webkit))
		progress = webkit_web_view_get_progress(webkit);

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
#if GTK_CHECK_VERSION(3, 0, 0)
	view = WEBKIT_WEB_VIEW(widget);
#else
	widget = gtk_bin_get_child(GTK_BIN(widget));
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
	WebKitWebView *webkit;
	const gchar *uri;

	webkit = webkit_browser_get_current_view(browser);
	priv = WEBKIT_BROWSER_GET_PRIVATE(browser);
	uri = gtk_entry_get_text(priv->entry);

	if (!webkit_web_view_is_loading(webkit))
		webkit_browser_load_uri(browser, uri);
	else
		webkit_web_view_stop_loading(webkit);
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

static void webkit_browser_view_stop(GtkWidget *widget, gpointer data)
{
	WebKitWebView *view;

#if GTK_CHECK_VERSION(3, 0, 0)
	view = WEBKIT_WEB_VIEW(widget);
#else
	widget = gtk_bin_get_child(GTK_BIN(widget));
	view = WEBKIT_WEB_VIEW(widget);
#endif

	/* FIXME: disconnect notify::progress signal handler instead? */
	webkit_web_view_stop_loading(view);
}

static gboolean on_destroy(GtkWidget *widget, gpointer data)
{
	WebKitBrowserPrivate *priv = WEBKIT_BROWSER_GET_PRIVATE(widget);

	/*
	 * Stop load operations on all WebKitWebView widgets because they may
	 * keep sending events to the URI entry progress bar, which may go
	 * away earlier.
	 */
	gtk_container_foreach(GTK_CONTAINER(priv->notebook),
			      webkit_browser_view_stop, NULL);

	return FALSE;
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

	g_debug("Webkit download started! Loading to: %s", filename);

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

static gboolean on_run_file_chooser(WebKitWebView *webkit,
				    WebKitFileChooserRequest *request,
				    gpointer data)
{
	return TRUE;
}

static gboolean on_webview_button_press_event(GtkWidget *widget,
					      GdkEvent  *event,
					      gpointer   data)
{
	WebKitBrowserPrivate *priv = WEBKIT_BROWSER_GET_PRIVATE(data);
	WebKitHitTestResult *result;
	guint context = 0;

	result = webkit_web_view_get_hit_test_result(WEBKIT_WEB_VIEW(widget), &event->button);
	g_assert(result);
	g_object_get(result, "context", &context, NULL);
	g_object_unref(result);

	if (context == 0)
		return FALSE;

	if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE)
		gtk_toggle_tool_button_set_active(priv->toggle, TRUE);
	else
		gtk_toggle_tool_button_set_active(priv->toggle, FALSE);

	return FALSE;
}
#endif

static gboolean webkit_browser_can_open_tab(WebKitBrowser *browser)
{
	WebKitBrowserPrivate *priv = WEBKIT_BROWSER_GET_PRIVATE(browser);
	gint pages = gtk_notebook_get_n_pages(priv->notebook);

	if (gtk_notebook_get_show_tabs(priv->notebook)) {
		/*
		 * The max-pages property is set only after the initialization
		 * of the widget, but this function may be called earlier when
		 * adding an initial WebKitWebView. In that case, max_pages
		 * will be 0, so account for this special case.
		 */
		if (!priv->max_pages || pages < priv->max_pages)
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
static gboolean webkit_decide_policy(WebKitWebView *web_view,
				     WebKitPolicyDecision *decision,
				     WebKitPolicyDecisionType type,
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
#if USE_WEBKIT2
static WebKitWebView *on_create_web_view(WebKitWebView *web_view,
#else
static WebKitWebView *on_create_web_view(WebKitWebView *web_view, WebKitWebFrame *frame,
#endif
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

#ifdef USE_WEBKIT2
static gchar *webkit_web_view_get_user_agent(WebKitWebView *webkit)
{
	WebKitSettings *settings;
	const gchar *user_agent;

	settings = webkit_web_view_get_settings(webkit);
	user_agent = webkit_settings_get_user_agent(settings);

	return g_strdup(user_agent);
}

static void webkit_web_view_set_user_agent(WebKitWebView *webkit,
					   const gchar *user_agent)
{
	WebKitSettings *settings;

	settings = webkit_web_view_get_settings(webkit);
	webkit_settings_set_user_agent(settings, user_agent);
}
#else
static gchar *webkit_web_view_get_user_agent(WebKitWebView *webkit)
{
	WebKitWebSettings *settings;
	const gchar *user_agent;

	settings = webkit_web_view_get_settings(webkit);
	user_agent = webkit_web_settings_get_user_agent(settings);

	return g_strdup(user_agent);
}

static void webkit_web_view_set_user_agent(WebKitWebView *webkit,
					   const gchar *user_agent)
{
	WebKitWebSettings *settings;

	settings = webkit_web_view_get_settings(webkit);
	g_object_set(G_OBJECT(settings), "user-agent", user_agent, NULL);
}
#endif

static void webkit_browser_set_user_agent(WebKitWebView *webkit, const gchar *str)
{
	gchar *user_agent, *ptr;

	if (str) {
		webkit_web_view_set_user_agent(webkit, str);
		return;
	}

	user_agent = webkit_web_view_get_user_agent(webkit);

	/*
	 * If armv7l is set as processor we simply remove it. This is an ugly
	 * workaround for some sites that redirect to mobile versions of the
	 * page for *any* ARM device.
	 */
	ptr = g_strstr_len(user_agent, -1, " armv7l");
	if (ptr) {
		gsize length = strlen(ptr + 7) + 1;

		memmove(ptr, ptr + 7, length);
	}

	webkit_web_view_set_user_agent(webkit, user_agent);
	g_free(user_agent);
}

static gint webkit_browser_append_tab(WebKitBrowser *browser, const gchar *title, gboolean hidden)
{
	WebKitBrowserPrivate *priv = WEBKIT_BROWSER_GET_PRIVATE(browser);
#ifdef USE_WEBKIT2
	WebKitWebContext *context = webkit_web_context_get_default();
#endif
	GtkWidget *webkit;
	GtkWidget *label;
	GtkWidget *view;
	gint page;

	if (!webkit_browser_can_open_tab(browser))
		return -1;

	/* create WebKit browser */
	webkit = webkit_web_view_new();
	webkit_browser_set_user_agent(WEBKIT_WEB_VIEW(webkit), priv->user_agent);
	gtk_widget_set_double_buffered(webkit, TRUE);

	/* TODO: Support GtkDragView with scrollbar */
#if !GTK_CHECK_VERSION(3, 0, 0)
	view = katze_scrolled_new(NULL, NULL);
	g_object_set(G_OBJECT(view), "drag-scrolling", TRUE, NULL);
	gtk_rc_parse_string(style_large);
	gtk_container_add(GTK_CONTAINER(view), webkit);
	gtk_widget_show(view);
#else
	view = webkit;
#endif
	if (!hidden)
		gtk_widget_show(webkit);

	label = webkit_browser_tab_label_new(title);
	gtk_widget_show(label);

	page = gtk_notebook_append_page(priv->notebook, view, label);

#if USE_WEBKIT2
	g_signal_connect(G_OBJECT(webkit), "decide-policy",
			G_CALLBACK(webkit_decide_policy), browser);
	g_signal_connect(G_OBJECT(context), "download-started",
			G_CALLBACK(webkit_download_started), browser);
	g_signal_connect(G_OBJECT(webkit), "notify::estimated-load-progress",
			G_CALLBACK(on_notify_progress), browser);
	g_signal_connect(G_OBJECT(webkit), "notify::create",
			G_CALLBACK(on_create_web_view), browser);
#else
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
	g_signal_connect(G_OBJECT(webkit), "create-web-view",
			G_CALLBACK(on_create_web_view), browser);
	g_signal_connect(G_OBJECT(webkit), "run-file-chooser",
			G_CALLBACK(on_run_file_chooser), browser);
#endif

	if (hidden)
		g_signal_connect(G_OBJECT(webkit), "web-view-ready",
				G_CALLBACK(on_web_view_ready), webkit);

	g_signal_connect(G_OBJECT(webkit), "button-press-event",
			G_CALLBACK(on_webview_button_press_event), browser);
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
	const gchar *uri, *icon;
	WebKitWebView *webkit;
#ifndef USE_WEBKIT2
	WebKitWebFrame *frame;
#endif
	GtkWidget *widget;

#if GTK_CHECK_VERSION(3, 0, 0)
	if (WEBKIT_IS_WEB_VIEW(page)) {
		/* the notebook page contains a WebKitWebView */
		widget = page;
#else
	if (GTK_IS_BIN(page)) {
		/* the notebook page contains a KatzeScrolled */
		widget = gtk_bin_get_child(GTK_BIN(page));
#endif

		webkit = WEBKIT_WEB_VIEW(widget);

#ifndef USE_WEBKIT2
		frame = webkit_web_view_get_main_frame(webkit);
#endif

		gtk_widget_set_sensitive(GTK_WIDGET(priv->back), TRUE);
		gtk_widget_set_sensitive(GTK_WIDGET(priv->forward), TRUE);
		gtk_widget_set_sensitive(GTK_WIDGET(priv->entry), TRUE);
		gtk_widget_set_sensitive(GTK_WIDGET(priv->reload), TRUE);

		if (!webkit_web_view_is_loading(webkit))
			icon = "go-jump";
		else
			icon = "stop";

		gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(priv->reload),
					      icon);

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

	priv->add_tab = gtk_button_new();
	image = gtk_image_new_from_stock(GTK_STOCK_ADD, GTK_ICON_SIZE_BUTTON);
	gtk_container_add(GTK_CONTAINER(priv->add_tab), image);
	g_signal_connect(G_OBJECT(priv->add_tab), "clicked",
			G_CALLBACK(on_add_tab_clicked), browser);
	gtk_box_pack_start(GTK_BOX(hbox), priv->add_tab, FALSE, FALSE, 0);
	gtk_widget_show_all(priv->add_tab);

	priv->del_tab = gtk_button_new();
	image = gtk_image_new_from_stock(GTK_STOCK_REMOVE,
			GTK_ICON_SIZE_BUTTON);
	gtk_container_add(GTK_CONTAINER(priv->del_tab), image);
	g_signal_connect(G_OBJECT(priv->del_tab), "clicked",
			G_CALLBACK(on_del_tab_clicked), browser);
	gtk_box_pack_start(GTK_BOX(hbox), priv->del_tab, FALSE, FALSE, 10);
	gtk_widget_show_all(priv->del_tab);

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
	WebKitWebView *webkit;
	GtkOskLayout *layout;
	GtkWidget *notebook;
	GtkWidget *toolbar;
	GtkWidget *vbox;

#ifndef USE_WEBKIT2
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
#else
	/*
	 * WebKit2 automatically enables the default proxy resolver and stores
	 * cookies in memory unless told otherwise.
	 */
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

	/*
	 * Make the browser widget grab the focus. Otherwise Gtk+ will focus
	 * the toolbar by default, which isn't very useful for an application
	 * designed for touchscreen use.
	 */
	webkit = webkit_browser_get_current_view(browser);
	gtk_widget_grab_focus(GTK_WIDGET(webkit));

	g_signal_connect(G_OBJECT(browser), "destroy", G_CALLBACK(on_destroy),
			 NULL);
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

	g_object_class_install_property(object, PROP_MAX_PAGES,
			g_param_spec_uint("max-pages",
					  "Maximum number of pages",
					  "Maximum number of pages",
					  1, 255, WEBKIT_BROWSER_MAX_PAGES,
					  G_PARAM_READWRITE |
					  G_PARAM_CONSTRUCT |
					  G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(object, PROP_USER_AGENT,
			g_param_spec_string("user-agent",
				"User-Agent string",
				"User-Agent string",
				NULL,
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
