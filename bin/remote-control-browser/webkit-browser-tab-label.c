/*
 * Copyright (C) 2012 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "webkit-browser-tab-label.h"

G_DEFINE_TYPE(WebKitBrowserTabLabel, webkit_browser_tab_label, GTK_TYPE_BOX);

typedef struct {
	GtkSpinner *spinner;
	GtkLabel *title;
} WebKitBrowserTabLabelPrivate;

#define WEBKIT_BROWSER_TAB_LABEL_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), WEBKIT_TYPE_BROWSER_TAB_LABEL, WebKitBrowserTabLabelPrivate))

static void webkit_browser_tab_label_init(WebKitBrowserTabLabel *self)
{
#if GTK_CHECK_VERSION(3, 16, 0)
	GtkCssProvider *provider = gtk_css_provider_new();
	GdkDisplay *display = gdk_display_get_default();
	GdkScreen *screen = gdk_display_get_default_screen(display);
#else
	PangoFontDescription *font_desc = pango_font_description_new();
#endif
	WebKitBrowserTabLabelPrivate *priv =
		WEBKIT_BROWSER_TAB_LABEL_GET_PRIVATE(self);
	GtkBox *box = GTK_BOX(self);
	GtkWidget *widget;

	gtk_box_set_homogeneous(box, FALSE);
	gtk_box_set_spacing(box, 2);

	widget = gtk_label_new("New Tab");
	gtk_label_set_ellipsize(GTK_LABEL(widget), PANGO_ELLIPSIZE_END);
	priv->title = GTK_LABEL(widget);

#if GTK_CHECK_VERSION(3, 16, 0)
	gtk_style_context_add_provider_for_screen(screen,
		GTK_STYLE_PROVIDER(provider),
		GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	gtk_css_provider_load_from_data(provider,
		"GtkLabel { font-size: 14pt; }", -1, NULL);
	g_object_unref(provider);
	gtk_widget_set_halign(widget, 0.0);
	gtk_widget_set_valign(widget, 0.5);
#else
	pango_font_description_set_size(font_desc, 14 * PANGO_SCALE);
#if GTK_CHECK_VERSION(3, 0, 0)
	gtk_widget_override_font(widget, font_desc);
	gtk_widget_set_halign(widget, 0.0);
	gtk_widget_set_valign(widget, 0.5);
#else
	gtk_widget_modify_font(widget, font_desc);
	gtk_misc_set_alignment(GTK_MISC(widget), 0.0, 0.5);
#endif
	pango_font_description_free(font_desc);
#endif /* GTK < 3.16.0 */

	gtk_widget_show(widget);

	gtk_box_pack_start(box, widget, TRUE, TRUE, 0);

	widget = gtk_spinner_new();
	priv->spinner = GTK_SPINNER(widget);
	gtk_widget_hide(widget);

	gtk_box_pack_start(box, widget, FALSE, FALSE, 0);
}

static void webkit_browser_tab_label_finalize(GObject *object)
{
	G_OBJECT_CLASS(webkit_browser_tab_label_parent_class)->finalize(object);
}

static void webkit_browser_tab_label_class_init(WebKitBrowserTabLabelClass *class)
{
	GObjectClass *object = G_OBJECT_CLASS(class);

	object->finalize = webkit_browser_tab_label_finalize;

	g_type_class_add_private(class, sizeof(WebKitBrowserTabLabelPrivate));
}

GtkWidget *webkit_browser_tab_label_new(const gchar *title)
{
	GtkWidget *widget = g_object_new(WEBKIT_TYPE_BROWSER_TAB_LABEL, NULL);
	WebKitBrowserTabLabelPrivate *priv =
		WEBKIT_BROWSER_TAB_LABEL_GET_PRIVATE(widget);

	if (title)
		gtk_label_set_text(priv->title, title);

	return widget;
}

void webkit_browser_tab_label_set_title(WebKitBrowserTabLabel *label,
		const gchar *title)
{
	WebKitBrowserTabLabelPrivate *priv =
		WEBKIT_BROWSER_TAB_LABEL_GET_PRIVATE(label);

	if (title)
		gtk_label_set_text(priv->title, title);
}

void webkit_browser_tab_label_set_loading(WebKitBrowserTabLabel *label,
		gboolean loading)
{
	WebKitBrowserTabLabelPrivate *priv =
		WEBKIT_BROWSER_TAB_LABEL_GET_PRIVATE(label);

	if(!priv)
		return;

	if (loading) {
		gtk_widget_show(GTK_WIDGET(priv->spinner));
		gtk_spinner_start(priv->spinner);
	} else {
		gtk_widget_hide(GTK_WIDGET(priv->spinner));
		gtk_spinner_stop(priv->spinner);
	}
}
