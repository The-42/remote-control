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

#include <webkit/webkit.h>
#include <gtk/gtk.h>

#include "remote-control-scrolled-window.h"

struct browser {
	GtkAdjustment *h;
	GtkAdjustment *v;
	gboolean pressed;
	gdouble x;
	gdouble y;
};

static gchar *geometry = NULL;

static GOptionEntry entries[] = {
	{
		"geometry", 'g', 0, G_OPTION_ARG_STRING, &geometry,
		"Window geometry", NULL
	}, { NULL }
};

void on_destroy(GtkObject *object, gpointer data)
{
	gtk_main_quit();
}

int main(int argc, char *argv[])
{
	GOptionContext *context;
	struct browser browser;
	WebKitWebView *webkit;
	GError *error = NULL;
	GtkWidget *window;
	GtkWidget *widget;
	GtkWidget *scroll;
	gboolean err;
	gchar *uri;

	context = g_option_context_new("- standalone browser");
	g_option_context_add_main_entries(context, entries, NULL);
	g_option_context_add_group(context, gtk_get_option_group(TRUE));

	if (!g_option_context_parse(context, &argc, &argv, &error)) {
		g_print("option parsing failed: %s\n", error->message);
		return 1;
	}

	g_option_context_free(context);

	memset(&browser, 0, sizeof(browser));

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(on_destroy),
			NULL);

	scroll = remote_control_scrolled_window_new(NULL, NULL);

	if (argc < 2)
		uri = "http://www.google.com/ncr";
	else
		uri = argv[1];

	widget = webkit_web_view_new();
	webkit = WEBKIT_WEB_VIEW(widget);
	webkit_web_view_load_uri(webkit, uri);

	gtk_container_add(GTK_CONTAINER(scroll), widget);
	gtk_container_add(GTK_CONTAINER(window), scroll);
	gtk_widget_show_all(scroll);

	if (geometry) {
		err = gtk_window_parse_geometry(GTK_WINDOW(window), geometry);
		if (!err)
			g_print("geometry parsing failed\n");
	}

	gtk_widget_show_all(window);
	gtk_main();

	return 0;
}
