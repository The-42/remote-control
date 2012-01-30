/*
 * Copyright (C) 2012 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef GKEYFILE_H
#define GKEYFILE_H 1

#include <gio/gio.h>

G_BEGIN_DECLS

typedef gboolean (*GFileMatchFunc)(GFile *file, gpointer user_data);

GKeyFile *g_key_file_new_from_path(const gchar *path, GKeyFileFlags flags,
		GError **error);
GKeyFile *g_key_file_new_from_file(GFile *file, GKeyFileFlags flags,
		GError **error);
gboolean g_key_file_merge(GKeyFile *dest, GKeyFile *source, GError **error);
GKeyFile *g_key_file_new_from_directory(const gchar *path,
		GFileMatchFunc match, gpointer user_data, GError **error);

G_END_DECLS

#endif /* GKEYFILE_H */
