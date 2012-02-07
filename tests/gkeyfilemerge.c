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

#include <glib-unix.h>
#include <gio/gio.h>
#include <glib.h>

#include "gkeyfile.h"

static gboolean match_glob(GFile *file, gpointer user_data)
{

	const gchar *glob = user_data;
	gboolean ret = FALSE;
	gchar *filename;

	filename = g_file_get_basename(file);

	if (g_pattern_match_simple(glob, filename))
		ret = TRUE;

	g_free(filename);
	return ret;
}

int main(int argc, char *argv[])
{
	GError *error = NULL;
	GKeyFile *file;
	GKeyFile *conf;
	int i;

	if (argc < 2) {
		g_printerr("usage: %s filename [directory...]\n", argv[0]);
		return 1;
	}

	g_type_init();

	conf = g_key_file_new();
	if (!conf) {
		g_debug("failed to create configuration");
		return 1;
	}

	for (i = 2; i < argc; i++) {
		file = g_key_file_new_from_directory(argv[i], match_glob, "*.conf", &error);
		if (!file && error) {
			g_debug("failed to load configuration from directory: %s",
					error->message);
			g_clear_error(&error);
		}

		if (!g_key_file_merge(conf, file, &error)) {
			g_debug("failed to merge configuration: %s", error->message);
			g_clear_error(&error);
		}
	}

	file = g_key_file_new_from_path(argv[1], G_KEY_FILE_NONE, &error);
	if (!file) {
		g_debug("failed to load file: %s", error->message);
		g_clear_error(&error);
	}

	if (!g_key_file_merge(conf, file, &error)) {
		g_debug("failed to merge configuration: %s", error->message);
		g_clear_error(&error);
	}

	g_key_file_free(file);
	g_key_file_free(conf);

	return 0;
}
