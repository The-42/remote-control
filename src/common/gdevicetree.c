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

#include <errno.h>
#include <string.h>

#include "gdevicetree.h"
#include "gkeyfile.h"

#define DT_PATH "/proc/device-tree"

struct GDeviceTree {
	gchar **model;
	gchar **compatible;
};

GQuark g_device_tree_error_quark(void)
{
	return g_quark_from_static_string("g-device-tree-error-quark");
}

static gchar **parse_string_list(const gchar *filename, guint *countp,
				 GError **errorp)
{
	gchar *data = NULL, **list = NULL;
	gsize size = 0, pos = 0, num = 0;
	GError *error = NULL;

	if (!g_file_get_contents(filename, &data, &size, &error)) {
		g_propagate_error(errorp, error);
		return NULL;
	}

	while (pos < size) {
		int len = strlen(data + pos);

		list = g_realloc(list, sizeof(*list) * (num + 2));
		if (!list) {
			g_set_error(errorp, G_DEVICE_TREE_ERROR,
				    G_DEVICE_TREE_ERROR_OUT_OF_MEMORY,
				    "out of memory");
			goto free;
		}

		list[num] = g_strdup(data + pos);
		if (!list[num]) {
			g_set_error(errorp, G_DEVICE_TREE_ERROR,
				    G_DEVICE_TREE_ERROR_OUT_OF_MEMORY,
				    "out of memory");
			goto free;
		}

		list[++num] = NULL;
		pos += len + 1;
	}

	if (countp)
		*countp = num;

	g_free(data);
	return list;

free:
	g_strfreev(list);
	return NULL;
}

static gboolean parse_model(GDeviceTree *dt, GError **errorp)
{
	GError *error = NULL;
	guint num;

	dt->model = parse_string_list(DT_PATH "/model", &num, &error);
	if (!dt->model) {
		g_propagate_error(errorp, error);
		return FALSE;
	}

	return TRUE;
}

static gboolean parse_compatible(GDeviceTree *dt, GError **errorp)
{
	GError *error = NULL;
	guint num;

	dt->compatible = parse_string_list(DT_PATH "/compatible", &num,
					   &error);
	if (!dt->compatible) {
		g_propagate_error(errorp, error);
		return FALSE;
	}

	return TRUE;
}

GDeviceTree *g_device_tree_load(GError **errorp)
{
	GError *error = NULL;
	GDeviceTree *dt;
	gboolean status;

	dt = g_new0(GDeviceTree, 1);
	if (!dt) {
		g_set_error(errorp, G_DEVICE_TREE_ERROR,
			    G_DEVICE_TREE_ERROR_OUT_OF_MEMORY,
			    "out of memory");
		return NULL;
	}

	status = parse_model(dt, &error);
	if (!status) {
		g_propagate_error(errorp, error);
		return NULL;
	}

	status = parse_compatible(dt, &error);
	if (!status) {
		g_propagate_error(errorp, error);
		return NULL;
	}

	return dt;
}

void g_device_tree_free(GDeviceTree *dt)
{
	if (!dt)
		return;

	g_strfreev(dt->model);
	g_free(dt);
}

gchar **g_device_tree_get_model(GDeviceTree *dt, guint *count)
{
	if (!dt)
		return NULL;

	if (count)
		*count = g_strv_length(dt->model);

	return g_strdupv(dt->model);
}

gchar **g_device_tree_get_compatible(GDeviceTree *dt, guint *count)
{
	if (!dt)
		return NULL;

	if (count)
		*count = g_strv_length(dt->compatible);

	return g_strdupv(dt->compatible);
}

gboolean g_device_tree_is_compatible(GDeviceTree *dt, const gchar *compatible)
{
	gchar **compat;

	if (!dt || !compatible)
		return FALSE;

	for (compat = dt->compatible; *compat; compat++)
		if (g_str_equal(*compat, compatible))
			return TRUE;

	return FALSE;
}
