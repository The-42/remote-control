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

#include <stdlib.h>
#include <string.h>

#include <poppler.h>
#include "gtk-pdf-view.h"

static const GtkBorder spacing = {
	.left = 8,
	.right = 8,
	.top = 8,
	.bottom = 8,
};

static const GtkBorder border = {
	.left = 1,
	.right = 1,
	.top = 1,
	.bottom = 1,
};

static const gint max_page_chars = 5;
static const double scale = 1.0;
static const gint shadow = 4;

enum {
	PROP_0,
	PROP_TITLE,
	PROP_LOADING,
};

typedef struct {
	PopplerDocument *document;
	GtkDrawingArea *canvas;
	GtkToolItem *forward;
	GtkToolbar *toolbar;
	GtkToolItem *back;
	gboolean loading;
	GtkEntry *entry;
	GtkLabel *label;
	gchar *title;
	int page;
} GtkPdfViewPrivate;

#define GTK_PDF_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), GTK_TYPE_PDF_VIEW, GtkPdfViewPrivate))

G_DEFINE_TYPE(GtkPdfView, gtk_pdf_view, GTK_TYPE_BOX);

static void gtk_pdf_view_add_child(GtkContainer *container, GtkWidget *child)
{
	g_debug("> %s(container=%p, child=%p)", __func__, container, child);
	g_debug("< %s()", __func__);
}

static void gtk_pdf_view_remove_child(GtkContainer *container, GtkWidget *child)
{
	g_debug("> %s(container=%p, child=%p)", __func__, container, child);
	g_debug("< %s()", __func__);
}

static void update_canvas_size(GtkWidget *widget, double width, double height)
{
	gint w = spacing.left + border.left + width + border.right + shadow + spacing.right;
	gint h = spacing.top + border.top + height + border.bottom + shadow + spacing.bottom;

	gtk_widget_set_size_request(widget, w, h);
	gtk_widget_queue_draw(widget);
}

static void update_toolbar(GtkWidget *widget)
{
	GtkPdfViewPrivate *priv;
	gint num_pages;
	gchar *buffer;

	g_return_if_fail(GTK_IS_PDF_VIEW(widget));

	priv = GTK_PDF_VIEW_GET_PRIVATE(widget);
	num_pages = poppler_document_get_n_pages(priv->document);

	buffer = g_strdup_printf("%u", priv->page + 1);
	gtk_entry_set_text(priv->entry, buffer);
	g_free(buffer);

	gtk_widget_set_sensitive(GTK_WIDGET(priv->back),
			priv->page > 0);
	gtk_widget_set_sensitive(GTK_WIDGET(priv->forward),
			priv->page < (num_pages - 1));
}

static void goto_page(GtkPdfView *view, gint pgno)
{
	GtkPdfViewPrivate *priv = GTK_PDF_VIEW_GET_PRIVATE(view);

	if (priv->document) {
		gint num_pages = poppler_document_get_n_pages(priv->document);
		PopplerPage *page;
		double height;
		double width;

		pgno = CLAMP(pgno, 0, num_pages - 1);
		if (pgno != priv->page) {
			g_debug("switching to page %u", priv->page);
			priv->page = pgno;

			page = poppler_document_get_page(priv->document,
					priv->page);
			poppler_page_get_size(page, &width, &height);
			g_object_unref(page);

			update_canvas_size(GTK_WIDGET(priv->canvas),
					width * scale, height * scale);
			update_toolbar(GTK_WIDGET(view));
		}
	}
}

static void on_back_clicked(GtkWidget *widget, gpointer data)
{
	GtkPdfViewPrivate *priv = GTK_PDF_VIEW_GET_PRIVATE(data);

	goto_page(GTK_PDF_VIEW(data), priv->page - 1);
}

static void on_forward_clicked(GtkWidget *widget, gpointer data)
{
	GtkPdfViewPrivate *priv = GTK_PDF_VIEW_GET_PRIVATE(data);

	goto_page(GTK_PDF_VIEW(data), priv->page + 1);
}

static void on_page_activate(GtkWidget *widget, gpointer data)
{
	GtkPdfViewPrivate *priv = GTK_PDF_VIEW_GET_PRIVATE(data);
	const gchar *number;
	unsigned long page;
	gchar *end = NULL;

	number = gtk_entry_get_text(priv->entry);

	page = strtoul(number, &end, 10);
	if (end == number) {
		g_debug("%s(): not a number: %s", __func__, number);
		return;
	}

	goto_page(GTK_PDF_VIEW(data), page - 1);
}

static GtkWidget *gtk_pdf_view_create_toolbar(GtkPdfView *self)
{
	GtkPdfViewPrivate *priv = GTK_PDF_VIEW_GET_PRIVATE(self);
	GtkWidget *toolbar;
	GtkToolItem *item;
	GtkWidget *widget;

	toolbar = gtk_toolbar_new();
	priv->toolbar = GTK_TOOLBAR(toolbar);

	gtk_toolbar_set_style(priv->toolbar, GTK_TOOLBAR_ICONS);
	//gtk_toolbar_set_icon_size(priv->toolbar, GTK_ICON_SIZE_DIALOG);

	priv->back = item = gtk_tool_button_new_from_stock(GTK_STOCK_GO_BACK);
	g_signal_connect(G_OBJECT(item), "clicked", G_CALLBACK(on_back_clicked), self);
	gtk_toolbar_insert(priv->toolbar, item, -1);
	gtk_widget_show(GTK_WIDGET(item));

	priv->forward = item = gtk_tool_button_new_from_stock(GTK_STOCK_GO_FORWARD);
	g_signal_connect(G_OBJECT(item), "clicked", G_CALLBACK(on_forward_clicked), self);
	gtk_toolbar_insert(priv->toolbar, item, -1);
	gtk_widget_show(GTK_WIDGET(item));

	/* page number entry */
	widget = gtk_entry_new();
	priv->entry = GTK_ENTRY(widget);
	gtk_entry_set_max_length(priv->entry, max_page_chars);
	gtk_entry_set_width_chars(priv->entry, max_page_chars);
	gtk_entry_set_alignment(priv->entry, 0.5);
	g_signal_connect(G_OBJECT(widget), "activate",
			G_CALLBACK(on_page_activate), self);
	gtk_widget_show(widget);

	item = gtk_tool_item_new();
	gtk_container_add(GTK_CONTAINER(item), widget);
	gtk_toolbar_insert(priv->toolbar, item, -1);
	gtk_widget_show(GTK_WIDGET(item));

	/* number of pages label */
	widget = gtk_label_new(NULL);
	priv->label = GTK_LABEL(widget);
	gtk_widget_show(widget);

	item = gtk_tool_item_new();
	gtk_container_add(GTK_CONTAINER(item), widget);
	gtk_toolbar_insert(priv->toolbar, item, -1);
	gtk_widget_show(GTK_WIDGET(item));

	return toolbar;
}

static gboolean on_canvas_expose(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	GtkPdfViewPrivate *priv = GTK_PDF_VIEW_GET_PRIVATE(data);
	GdkWindow *window;
	PopplerPage *page;
	cairo_t *cairo;

	if (priv->document) {
		double height;
		double width;

		window = gtk_widget_get_window(widget);
		page = poppler_document_get_page(priv->document, priv->page);
#if GTK_CHECK_VERSION(2, 91, 6)
		cairo = gdk_cairo_create(window);
#else
		cairo = gdk_cairo_create(GDK_DRAWABLE(window));
#endif

		poppler_page_get_size(page, &width, &height);
		width *= scale;
		height *= scale;

		/*
		 * TODO: This can probably be optimized by only drawing the
		 *       border where it is not overlapped by the actual PDF
		 *       content.
		 */

		cairo_set_source_rgb(cairo, 0.0, 0.0, 0.0);
		cairo_rectangle(cairo, spacing.left, spacing.top,
				width + border.left + border.right,
				height + border.top + border.bottom);
		cairo_rectangle(cairo, spacing.left + shadow,
				spacing.top + shadow,
				width + border.left + border.right,
				height + border.top + border.bottom);
		cairo_fill(cairo);

		cairo_set_source_rgb(cairo, 1.0, 1.0, 1.0);
		cairo_rectangle(cairo, spacing.left + border.left,
				spacing.top + border.top,
				width, height);
		cairo_fill(cairo);

		cairo_scale(cairo, scale, scale);
		poppler_page_render(page, cairo);

		cairo_destroy(cairo);
		g_object_unref(page);
	}

	return TRUE;
}

static void gtk_pdf_view_init(GtkPdfView *self)
{
	GtkPdfViewPrivate *priv = GTK_PDF_VIEW_GET_PRIVATE(self);
	GtkBox *box = GTK_BOX(self);
	GtkWidget *toolbar;
	GtkWidget *window;
	GtkWidget *canvas;

	gtk_orientable_set_orientation(GTK_ORIENTABLE(self),
			GTK_ORIENTATION_VERTICAL);
	gtk_box_set_homogeneous(box, FALSE);
	gtk_box_set_spacing(box, 0);

	toolbar = gtk_pdf_view_create_toolbar(self);
	gtk_box_pack_start(box, toolbar, FALSE, FALSE, 0);
	gtk_widget_show(toolbar);

	window = gtk_scrolled_window_new(NULL, NULL);

	canvas = gtk_drawing_area_new();
	gtk_widget_set_app_paintable(canvas, TRUE);
#if GTK_CHECK_VERSION(2, 91, 0)
	g_signal_connect(G_OBJECT(canvas), "draw",
#else
	g_signal_connect(G_OBJECT(canvas), "expose-event",
#endif
			G_CALLBACK(on_canvas_expose), self);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(window),
			canvas);
	gtk_widget_show(canvas);

	gtk_box_pack_start(box, window, TRUE, TRUE, 0);
	gtk_widget_show(window);

	gtk_widget_show_all(GTK_WIDGET(box));

	priv->canvas = GTK_DRAWING_AREA(canvas);
	priv->page = 0;
}

#if GTK_CHECK_VERSION(2, 91, 0)
static gboolean gtk_pdf_view_draw(GtkWidget *widget, cairo_t *cr)
{
	if (gtk_widget_is_drawable(widget))
		GTK_WIDGET_CLASS(gtk_pdf_view_parent_class)->draw(widget, cr);

	return FALSE;
}
#else
static gboolean gtk_pdf_view_expose(GtkWidget *widget, GdkEventExpose *event)
{
	if (gtk_widget_is_drawable(widget))
		GTK_WIDGET_CLASS(gtk_pdf_view_parent_class)->expose_event(widget, event);

	return FALSE;
}
#endif

static void gtk_pdf_view_get_property(GObject *object, guint prop_id,
		GValue *value, GParamSpec *pspec)
{
	GtkPdfViewPrivate *priv = GTK_PDF_VIEW_GET_PRIVATE(object);

	switch (prop_id) {
	case PROP_TITLE:
		g_value_set_string(value, priv->title);
		break;

	case PROP_LOADING:
		g_value_set_boolean(value, priv->loading);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void gtk_pdf_view_set_property(GObject *object, guint prop_id,
		const GValue *value, GParamSpec *pspec)
{
	switch (prop_id) {
	case PROP_TITLE:
		g_debug("property \"title\" is read-only");
		break;

	case PROP_LOADING:
		g_debug("property \"loading\" is read-only");
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void gtk_pdf_view_finalize(GObject *object)
{
	GtkPdfViewPrivate *priv = GTK_PDF_VIEW_GET_PRIVATE(object);

	g_object_unref(priv->document);
	g_free(priv->title);

	G_OBJECT_CLASS(gtk_pdf_view_parent_class)->finalize(object);
}

static void gtk_pdf_view_class_init(GtkPdfViewClass *class)
{
	GtkContainerClass *container = GTK_CONTAINER_CLASS(class);
	GtkWidgetClass *widget = GTK_WIDGET_CLASS(class);
	GObjectClass *object = G_OBJECT_CLASS(class);

	container->add = gtk_pdf_view_add_child;
	container->remove = gtk_pdf_view_remove_child;

#if GTK_CHECK_VERSION(2, 91, 0)
	widget->draw = gtk_pdf_view_draw;
#else
	widget->expose_event = gtk_pdf_view_expose;
#endif

	object->get_property = gtk_pdf_view_get_property;
	object->set_property = gtk_pdf_view_set_property;
	object->finalize = gtk_pdf_view_finalize;

	g_object_class_install_property(object, PROP_TITLE,
			g_param_spec_string("title", "Document Title",
				"The document title.", NULL,
				G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(object, PROP_LOADING,
			g_param_spec_boolean("loading", "loading status",
				"The document loading status.", FALSE,
				G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	g_type_class_add_private(class, sizeof(GtkPdfViewPrivate));
}

static void on_download_status(WebKitDownload *download, GParamSpec *pspec,
		gpointer data)
{
	WebKitDownloadStatus status = webkit_download_get_status(download);
	const gchar *uri = webkit_download_get_destination_uri(download);
	GtkPdfView *view = GTK_PDF_VIEW(data);
	gboolean remove = FALSE;

	switch (status) {
	case WEBKIT_DOWNLOAD_STATUS_ERROR:
		g_debug("download failed");
		remove = TRUE;
		break;

	case WEBKIT_DOWNLOAD_STATUS_CREATED:
		g_debug("download created");
		break;

	case WEBKIT_DOWNLOAD_STATUS_STARTED:
		g_debug("download started");
		break;

	case WEBKIT_DOWNLOAD_STATUS_CANCELLED:
		g_debug("download cancelled");
		remove = TRUE;
		break;

	case WEBKIT_DOWNLOAD_STATUS_FINISHED:
		gtk_pdf_view_load_uri(view, uri);
		g_debug("download finished");
		remove = TRUE;
		break;
	}

	if (remove) {
		const gchar *filename = uri + strlen("file://");
		g_debug("removing file %s", filename);
		unlink(filename);
	}
}

GtkWidget *gtk_pdf_view_new(WebKitDownload *download)
{
	GtkWidget *widget = g_object_new(GTK_TYPE_PDF_VIEW, NULL);
	g_signal_connect(download, "notify::status", G_CALLBACK(on_download_status), widget);
	return widget;
}

gboolean gtk_pdf_view_load_uri(GtkPdfView *view, const gchar *uri)
{
	PopplerDocument *document;
	GtkPdfViewPrivate *priv;
	GError *error = NULL;
	PopplerPage *page;
	gint num_pages;
	gchar *buffer;
	double height;
	double width;

	g_return_val_if_fail(GTK_IS_PDF_VIEW(view), FALSE);
	priv = GTK_PDF_VIEW_GET_PRIVATE(view);

	document = poppler_document_new_from_file(uri, NULL, &error);
	if (!document) {
		g_debug("failed to load document: %s", error->message);
		g_error_free(error);
	}

	num_pages = poppler_document_get_n_pages(document);
	priv->document = document;
	priv->page = 0;

	priv->title = poppler_document_get_title(priv->document);
	if (!priv->title) {
		priv->title = poppler_document_get_subject(priv->document);
		if (!priv->title)
			priv->title = g_strdup_printf("Untitled document");
	}

	g_object_notify(G_OBJECT(view), "title");

	priv->loading = FALSE;
	g_object_notify(G_OBJECT(view), "loading");

	page = poppler_document_get_page(priv->document, priv->page);
	poppler_page_get_size(page, &width, &height);
	g_object_unref(page);

	buffer = g_strdup_printf(" / %u", num_pages);
	gtk_label_set_text(priv->label, buffer);
	g_free(buffer);

	update_canvas_size(GTK_WIDGET(priv->canvas), width * scale,
			height * scale);
	update_toolbar(GTK_WIDGET(view));

	return FALSE;
}
