/*
 * Copyright (C) 2012 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __G_DEVICE_TREE_H__
#define __G_DEVICE_TREE_H__

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
	G_DEVICE_TREE_ERROR_OUT_OF_MEMORY,
} GDeviceTreeError;

#define G_DEVICE_TREE_ERROR g_device_tree_error_quark()
GQuark g_device_tree_error_quark(void);

typedef struct GDeviceTree GDeviceTree;

GDeviceTree *g_device_tree_load(GError **error);
void g_device_tree_free(GDeviceTree *dt);
gchar **g_device_tree_get_model(GDeviceTree *dt, guint *count);
gchar **g_device_tree_get_compatible(GDeviceTree *dt, guint *count);
gboolean g_device_tree_is_compatible(GDeviceTree *dt, const gchar *compatible);

G_END_DECLS

#endif /* __G_DEVICE_TREE_H__ */
