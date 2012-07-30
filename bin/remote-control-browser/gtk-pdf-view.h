/*
 * Copyright (C) 2011-2012 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef GTK_PDF_VIEW_H
#define GTK_PDF_VIEW_H 1

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef USE_WEBKIT2
#include <webkit2/webkit2.h>
#else
#include <webkit/webkit.h>
#endif
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GTK_TYPE_PDF_VIEW (gtk_pdf_view_get_type())
#define GTK_IS_PDF_VIEW(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_PDF_VIEW))
#define GTK_PDF_VIEW(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_PDF_VIEW, GtkPdfView))

typedef struct GtkPdfView GtkPdfView;
typedef struct GtkPdfViewClass GtkPdfViewClass;

struct GtkPdfView {
	GtkBox parent;
};

struct GtkPdfViewClass {
	GtkBoxClass parent_class;
};

GType gtk_pdf_view_get_type(void) G_GNUC_CONST;
GtkWidget *gtk_pdf_view_new(WebKitDownload *download);
gboolean gtk_pdf_view_load_uri(GtkPdfView *view, const gchar *uri);

G_END_DECLS

#endif /* GTK_PDF_VIEW_H */
