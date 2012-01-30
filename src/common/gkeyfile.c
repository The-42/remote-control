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
#include "gkeyfile.h"

GKeyFile *g_key_file_new_from_path(const gchar *path, GKeyFileFlags flags,
		GError **error)
{
	GKeyFile *keyfile;
	gboolean status;

	keyfile = g_key_file_new();
	if (!keyfile) {
		g_set_error(error, G_UNIX_ERROR, ENOMEM, "out of memory");
		return NULL;
	}

	status = g_key_file_load_from_file(keyfile, path, G_KEY_FILE_NONE,
			error);
	if (!status) {
		g_key_file_free(keyfile);
		return NULL;
	}

	return keyfile;
}

GKeyFile *g_key_file_new_from_file(GFile *file, GKeyFileFlags flags,
		GError **error)
{
	GKeyFile *keyfile;
	gchar *path;

	path = g_file_get_path(file);
	if (!path) {
		g_set_error(error, G_UNIX_ERROR, EINVAL, "invalid argument");
		return NULL;
	}

	keyfile = g_key_file_new_from_path(path, flags, error);

	g_free(path);
	return keyfile;
}

gboolean g_key_file_merge(GKeyFile *dest, GKeyFile *source, GError **error)
{
	GError *e = NULL;
	gchar **groups;
	gchar **group;

	groups = g_key_file_get_groups(source, NULL);
	if (!groups)
		return TRUE;

	for (group = groups; *group; group++) {
		gchar **keys;
		gchar **key;

		keys = g_key_file_get_keys(source, *group, NULL, &e);
		if (!keys) {
			g_debug("g_key_file_get_keys(): %s", e->message);
			g_clear_error(&e);
			continue;
		}

		for (key = keys; *key; key++) {
			gchar *value;

			value = g_key_file_get_value(source, *group, *key, &e);
			if (!value) {
				g_debug("g_key_file_get_value(): %s", e->message);
				g_clear_error(&e);
				continue;
			}

			g_key_file_set_value(dest, *group, *key, value);
			g_free(value);
		}

		g_strfreev(keys);
	}

	g_strfreev(groups);
	return TRUE;
}

GKeyFile *g_key_file_new_from_directory(const gchar *path,
		GFileMatchFunc match, gpointer user_data, GError **error)
{
	GFileEnumerator *children;
	GError *e = NULL;
	GFile *directory;
	GKeyFile *conf;

	conf = g_key_file_new();
	if (!conf) {
		g_set_error(error, G_UNIX_ERROR, ENOMEM, "out of memory");
		return NULL;
	}

	directory = g_file_new_for_path(path);
	if (!directory) {
		g_set_error(error, G_UNIX_ERROR, ENOMEM, "out of memory");
		g_key_file_free(conf);
		return NULL;
	}

	children = g_file_enumerate_children(directory,
			G_FILE_ATTRIBUTE_STANDARD_NAME,
			G_FILE_QUERY_INFO_NONE, NULL, &e);
	if (!children) {
		g_propagate_error(error, e);
		return NULL;
	}

	while (TRUE) {
		GFileInfo *info;
		GKeyFile *file;
		GFile *child;

		info = g_file_enumerator_next_file(children, NULL, &e);
		if (!info) {
			if (e) {
				g_debug("failed to get child: %s", e->message);
				g_clear_error(&e);
				continue;
			}

			break;
		}

		child = g_file_get_child(directory, g_file_info_get_name(info));

		if (match && !match(child, user_data)) {
			g_object_unref(child);
			g_object_unref(info);
			continue;
		}

		g_debug("loading file: %s", g_file_get_path(child));

		file = g_key_file_new_from_file(child, G_KEY_FILE_NONE, &e);
		if (!file) {
			g_debug("failed to load configuration: %s", e->message);
			g_clear_error(&e);
		} else {
			if (!g_key_file_merge(conf, file, &e)) {
				g_debug("failed to merge configuration: %s", e->message);
				g_clear_error(&e);
			}
		}

		g_key_file_free(file);
		g_object_unref(child);
		g_object_unref(info);
	}

	g_object_unref(children);
	g_object_unref(directory);

	return conf;
}
