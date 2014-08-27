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

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/resource.h>
#include <gtk/gtk.h>

#include "webkit-browser.h"
#include "gkeyfile.h"
#include "utils.h"

static const gchar default_configfile[] = SYSCONF_DIR "/browser.conf";
static const gchar *configfile = default_configfile;
static GData *user_agent_overrides = NULL;
static gboolean disable_jshooks = FALSE;
static gchar *user_agent = NULL;
static gboolean adblock = FALSE;
static gboolean cursor = FALSE;
static gboolean noexit = FALSE;
static gchar *geometry = NULL;
static gboolean kiosk = FALSE;
static gchar *language = NULL;
static glong max_pages = 0;

static GOptionEntry entries[] = {
	{
		"config", 'f', 0, G_OPTION_ARG_FILENAME, &configfile,
		"Configuration file", NULL
	},
	{
		"geometry", 'g', 0, G_OPTION_ARG_STRING, &geometry,
		"Window geometry", NULL
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
	}, {
		"no-exit", 'E', 0, G_OPTION_ARG_NONE, &noexit,
		"No exit button", NULL
	}, {
		"user-agent", 'u', 0, G_OPTION_ARG_STRING, &user_agent,
		"User-Agent string", NULL
	}, {
		"max-pages", 'p', 0, G_OPTION_ARG_INT, &max_pages,
		"The max numbers of pages", NULL
	}, {
		"adblock", 'a', 0, G_OPTION_ARG_NONE, &adblock,
		"Use adblocker", NULL
	}, {
		"disable jshooks", 'j', 0, G_OPTION_ARG_NONE, &disable_jshooks,
		"Do not use JavaScript hook scripts", NULL
	}, {
		NULL
	}
};

static void on_destroy(GtkWidget *widget, gpointer data)
{
	GMainLoop *loop = data;

	g_main_loop_quit(loop);
}

static void on_realize(GtkWidget *widget, gpointer data)
{
	gdk_window_set_type_hint(gtk_widget_get_window(widget),
			GDK_WINDOW_TYPE_HINT_DESKTOP);
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

static void on_uri_notify(GObject *object, GParamSpec *spec, gpointer data)
{
	struct watchdog *watchdog = data;
	gchar *message;

	message = webkit_browser_get_uri(WEBKIT_BROWSER(object));
	g_debug("watchdog: new message: %s", message);
	watchdog_set_message(watchdog, g_strdelimit(message, "%", '#'));
	g_free(message);
}

static int parse_mem(const gchar *s, size_t *size)
{
	size_t value, unit = 1;
	gchar *end;

	value = strtoul(s, &end, 10);
	if (end == s)
		return -EINVAL;

	switch (*end) {
	case 'G':
		unit *= 1024;

	case 'M':
		unit *= 1024;

	case 'K':
		unit *= 1024;
		break;

	default:
		return -EINVAL;
	}

	*size = value * unit;

	return 0;
}

static void set_memory_limits(GKeyFile *keyfile)
{
	size_t limit = 0;
	struct rlimit l;
	gchar *value;
	int err;

	if (!keyfile || !g_key_file_has_group(keyfile, "limits"))
		return;

	value = g_key_file_get_string(keyfile, "limits", "memory", NULL);
	if (!value) {
		g_warning("\"memory\" key not found");
		return;
	}

	err = parse_mem(value, &limit);
	if (err < 0) {
		g_warning("failed to parse memory value: %s", strerror(-err));
		g_free(value);
		return;
	}

	g_free(value);

	l.rlim_cur = l.rlim_max = limit;

	err = setrlimit(RLIMIT_AS, &l);
	if (err < 0)
		g_warning("failed to set memory limit: %s", strerror(-err));
}

GKeyFile *load_configuration(const gchar *filename, GError **error)
{
	GKeyFile *keyfile;

	keyfile = g_key_file_new_from_path(filename, G_KEY_FILE_NONE, error);
	if (!keyfile)
		return NULL;

	if (g_key_file_has_group(keyfile, "browser")) {
		if (!user_agent) {
			user_agent = g_key_file_get_string(keyfile, "browser",
					"user-agent", error);
			g_clear_error(error);
		}

		if (!disable_jshooks) {
			disable_jshooks = !g_key_file_get_boolean(keyfile,
					"browser", "jshooks", NULL);
		}
	}

	if (g_key_file_has_group(keyfile, "limits")) {
		if (g_key_file_has_key(keyfile, "limits", "pages", NULL)) {
			max_pages = g_key_file_get_integer(keyfile, "limits",
					"pages", NULL);
			g_clear_error(error);
		}
	}

	if (g_key_file_has_group(keyfile, "localization")) {
		if (!language && g_key_file_has_key(keyfile, "localization",
						    "languages", error)) {
			language = g_key_file_get_string(keyfile,
					"localization", "languages", error);
			if (!language) {
				g_key_file_free(keyfile);
				return NULL;
			}
		}

		g_clear_error(error);
	}

	if (g_key_file_has_group(keyfile, "user-agent-overrides")) {
		gchar **keys;
		gchar **key;

		if (!user_agent_overrides)
			g_datalist_init(&user_agent_overrides);

		keys = g_key_file_get_keys(keyfile, "user-agent-overrides",
					NULL, NULL);
		for (key = keys; *key != NULL; key++) {
			gchar *value = g_key_file_get_string(keyfile,
						"user-agent-overrides",
						*key, NULL);
			g_datalist_id_set_data_full(&user_agent_overrides,
					g_quark_from_string(*key),
					value, g_free);
		}
		g_strfreev(keys);
	}

	return keyfile;
}

int main(int argc, char *argv[])
{
	struct watchdog *watchdog;
	GOptionContext *options;
	GMainContext *context;
	GError *error = NULL;
	GtkWidget *browser;
	gchar *uri = NULL;
	GMainLoop *loop;
	GKeyFile *conf;

	if (!ENABLE_NLS)
		gtk_disable_setlocale();

	/* parse command-line */
	options = g_option_context_new("- standalone browser");
	g_option_context_add_main_entries(options, entries, NULL);
	g_option_context_add_group(options, gtk_get_option_group(TRUE));

	if (!g_option_context_parse(options, &argc, &argv, &error)) {
		g_print("option parsing failed: %s\n", error->message);
		return 1;
	}

	g_option_context_free(options);

	/* load configuration file */
	conf = load_configuration(configfile, &error);
	if (!conf) {
		g_printerr("failed to load `%s': %s\n", configfile,
				error->message);
		g_clear_error(&error);
	}

	set_memory_limits(conf);

	if (argc < 2)
		uri = "http://www.google.com/ncr";
	else
		uri = argv[1];

	/* setup main loop */
	loop = g_main_loop_new(NULL, FALSE);
	g_assert(loop != NULL);

	context = g_main_loop_get_context(loop);
	g_assert(context != NULL);

	/* activate watchdog */
	watchdog = watchdog_new(conf, NULL);
	if (watchdog) {
		g_debug("D-Bus Watchdog activated");
		watchdog_attach(watchdog, context);
	}

	/* create and show browser window */
	browser = webkit_browser_new(geometry);
	g_object_set(browser, "controls", !kiosk, NULL);
	g_object_set(browser, "accept-language", language, NULL);
	g_object_set(browser, "no-exit", noexit, NULL);
	g_object_set(browser, "user-agent", user_agent, NULL);
	g_object_set(browser, "adblock", adblock, NULL);
	g_object_set(browser, "jshooks", !disable_jshooks, NULL);
	g_object_set(browser, "user-agent-overrides", user_agent_overrides,
			NULL);

	if (max_pages > 0)
		g_object_set(browser, "max-pages", max_pages, NULL);

	g_signal_connect(G_OBJECT(browser), "destroy", G_CALLBACK(on_destroy),
			loop);
	g_signal_connect(G_OBJECT(browser), "realize", G_CALLBACK(on_realize),
			NULL);
	g_signal_connect(G_OBJECT(browser), "notify::uri", G_CALLBACK(on_uri_notify),
			watchdog);
	webkit_browser_load_uri(WEBKIT_BROWSER(browser), uri);

	gtk_widget_show(browser);

	g_main_loop_run(loop);

	watchdog_unref(watchdog);
	g_main_loop_unref(loop);

	if (conf)
		g_key_file_free(conf);

	if (user_agent_overrides) {
		g_datalist_clear(&user_agent_overrides);
		g_free(user_agent_overrides);
	}

	return 0;
}
