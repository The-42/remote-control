/*
 * Copyright (C) 2012 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef WEBKIT_BROWSER_TAB_LABEL_H
#define WEBKIT_BROWSER_TAB_LABEL_H 1

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_BROWSER_TAB_LABEL (webkit_browser_tab_label_get_type())
#define WEBKIT_IS_BROWSER_TAB_LABEL(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_BROWSER_TAB_LABEL))
#define WEBKIT_BROWSER_TAB_LABEL(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_BROWSER_TAB_LABEL, WebKitBrowserTabLabel))

typedef struct WebKitBrowserTabLabel WebKitBrowserTabLabel;
typedef struct WebKitBrowserTabLabelClass WebKitBrowserTabLabelClass;

struct WebKitBrowserTabLabel {
	GtkBox parent;
};

struct WebKitBrowserTabLabelClass {
	GtkBoxClass parent_class;
};

GType webkit_browser_tab_label_get_type(void) G_GNUC_CONST;

GtkWidget *webkit_browser_tab_label_new(const gchar *title);
void webkit_browser_tab_label_set_title(WebKitBrowserTabLabel *label,
		const gchar *title);
void webkit_browser_tab_label_set_loading(WebKitBrowserTabLabel *label,
		gboolean loading);

G_END_DECLS

#endif /* WEBKIT_BROWSER_TAB_LABEL_H */
