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

#include <webkit/webkit.h>
#include <gtk/gtk.h>

#include "webkit-browser.h"
#include "gkeyfile.h"

static gchar *geometry = NULL;
static gboolean noosk = FALSE;
static gboolean kiosk = FALSE;
static gchar *language = NULL;
static gboolean cursor = FALSE;

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
	}, {
		"accept-language", 'l', 0, G_OPTION_ARG_STRING, &language,
		"Accept-Language string", NULL
	}, {
		"show-cursor", 'c', 0, G_OPTION_ARG_NONE, &cursor,
		"Show cursor", NULL
	},
	{ NULL }
};

static void on_destroy(GtkWidget *widget, gpointer data)
{
	gtk_main_quit();
}

static void on_realize(GtkWidget *widget, gpointer data)
{
	if (!cursor) {
		GdkCursor *cursor = gdk_cursor_new(GDK_BLANK_CURSOR);
		GdkWindow *window = gtk_widget_get_window(widget);

		gdk_window_set_cursor(window, cursor);
#if GTK_CHECK_VERSION(2, 91, 1)
		g_object_unref(cursor);
#else
		gdk_cursor_unref(cursor);
#endif
	}
}

gboolean load_configuration(const gchar *filename, GError **error)
{
	GKeyFile *keyfile;

	keyfile = g_key_file_new_from_path(filename, G_KEY_FILE_NONE, error);
	if (!keyfile)
		return FALSE;

	if (g_key_file_has_group(keyfile, "localization")) {
		if (!language && g_key_file_has_key(keyfile, "localization",
						    "languages", error)) {
			language = g_key_file_get_string(keyfile,
					"localization", "languages", error);
			if (!language) {
				g_key_file_free(keyfile);
				return FALSE;
			}
		}

		g_clear_error(error);
	}

	g_key_file_free(keyfile);
	return TRUE;
}

int main(int argc, char *argv[])
{
	static const gchar configfile[] = SYSCONF_DIR "/browser.conf";
	GOptionContext *options;
	GError *error = NULL;
	GtkWidget *browser;
	gchar *uri = NULL;

	/* parse command-line */
	options = g_option_context_new("- standalone browser");
	g_option_context_add_main_entries(options, entries, NULL);
	g_option_context_add_group(options, gtk_get_option_group(TRUE));

	if (!g_option_context_parse(options, &argc, &argv, &error)) {
		g_print("option parsing failed: %s\n", error->message);
		return 1;
	}

	g_option_context_free(options);

	if (!load_configuration(configfile, &error)) {
		g_printerr("failed to load `%s': %s\n", configfile,
				error->message);
		g_clear_error(&error);
	}

	if (argc < 2)
		uri = "http://www.google.com/ncr";
	else
		uri = argv[1];

	browser = webkit_browser_new(geometry);
	g_object_set(browser, "keyboard", !noosk, NULL);
	g_object_set(browser, "controls", !kiosk, NULL);
	g_object_set(browser, "accept-language", language, NULL);

	g_signal_connect(G_OBJECT(browser), "destroy", G_CALLBACK(on_destroy),
			NULL);
	g_signal_connect(G_OBJECT(browser), "realize", G_CALLBACK(on_realize),
			NULL);
	webkit_browser_load_uri(WEBKIT_BROWSER(browser), uri);

	gtk_widget_show(browser);

	gtk_main();

	return 0;
}
