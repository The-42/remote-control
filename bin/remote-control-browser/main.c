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

#include "webkit-browser.h"

static gchar *geometry = NULL;

static GOptionEntry entries[] = {
	{
		"geometry", 'g', 0, G_OPTION_ARG_STRING, &geometry,
		"Window geometry", NULL
	},
	{
		"noosk", 'o', 0, G_OPTION_ARG_NONE, &noosk,
		"No on screen keyboard", NULL
	},
	{
		"kiosk", 'k', 0, G_OPTION_ARG_NONE, &kiosk,
		"Run in kiosk mode", NULL
	},
	{ NULL }
};

static void on_destroy(GtkObject *object, gpointer data)
{
	gtk_main_quit();
}

static void on_realize(GtkWidget *widget, gpointer data)
{
//TODO: just uncommented to have a mouse cursor during testing on pc 
//      reenable these lines to hide cursor on Medcom terminal
//	GdkCursor *cursor = gdk_cursor_new(GDK_BLANK_CURSOR);
//	gdk_window_set_cursor(widget->window, cursor);
//	gdk_cursor_unref(cursor);
}

int main(int argc, char *argv[])
{
	GOptionContext *context;
	GError *error = NULL;
	GtkWidget *browser;
	gchar *uri = NULL;
//	int i = 0;

	context = g_option_context_new("- standalone browser");
	g_option_context_add_main_entries(context, entries, NULL);
	g_option_context_add_group(context, gtk_get_option_group(TRUE));

	if (!g_option_context_parse(context, &argc, &argv, &error)) {
		g_print("option parsing failed: %s\n", error->message);
		return 1;
	}

	g_option_context_free(context);
#if 0
	printf("argc=%d\n", argc);
	for(i = 1; i < argc; i++)
	{
		printf("argv[i]=%s\n", argv[i]);
		if(strncmp(argv[i], "noosk", 5) == 0)
		{
			printf("noosk\n");
			noosk = TRUE;
		}
		else if(strncmp(argv[i], "kiosk", 5) == 0)
		{
			printf("kiosk\n");
			kiosk = TRUE;
		}
		else
			uri = argv[i];
	}
#endif	
	if (argc < 2)
		uri = "http://www.google.com/ncr";
	else
		uri = argv[1];

	browser = webkit_browser_new(geometry);
	g_signal_connect(G_OBJECT(browser), "destroy", G_CALLBACK(on_destroy),
			NULL);
	g_signal_connect(G_OBJECT(browser), "realize", G_CALLBACK(on_realize),
			NULL);
	webkit_browser_load_uri(WEBKIT_BROWSER(browser), uri);
	gtk_widget_show_all(browser);

	gtk_main();

	return 0;
}
