/*
 * Copyright (C) 2011 - 2013 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef WEBKIT_BROWSER_H
#define WEBKIT_BROWSER_H 1

#include <gtk/gtk.h>
#ifdef USE_WEBKIT2
#include <webkit2/webkit2.h>
#else
#include <webkit/webkit.h>
#endif

G_BEGIN_DECLS

#define WEBKIT_TYPE_BROWSER (webkit_browser_get_type())
#define WEBKIT_BROWSER(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_BROWSER, WebKitBrowser))

typedef struct _WebKitBrowser        WebKitBrowser;
typedef struct _WebKitBrowserClass   WebKitBrowserClass;
typedef struct _WebKitBrowserPrivate WebKitBrowserPrivate;

struct _WebKitBrowser {
	GtkWindow parent;
};

struct _WebKitBrowserClass {
	GtkWindowClass parent_class;
};

GType webkit_browser_get_type(void) G_GNUC_CONST;

GtkWidget *webkit_browser_new(const gchar *geometry);
void webkit_browser_load_uri(WebKitBrowser *browser, const gchar *uri);
gchar *webkit_browser_get_uri(WebKitBrowser *browser);

GtkNotebook *webkit_browser_get_tabs (WebKitBrowser *browser);
WebKitWebView *webkit_browser_get_web_view (WebKitBrowser *browser);

G_END_DECLS

#endif /* WEBKIT_BROWSER_H */
