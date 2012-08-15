/*
 * Copyright (C) 2012 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __G_SYSFS_H__
#define __G_SYSFS_H__

#include <glib-object.h>
#include <gudev/gudev.h>

G_BEGIN_DECLS

typedef enum {
	G_SYSFS_ERROR_PARSE,
	G_SYSFS_ERROR_DEVICE_NOT_FOUND,
	G_SYSFS_ERROR_INVALID_VALUE,
	G_SYSFS_ERROR_OUT_OF_MEMORY,
	G_SYSFS_ERROR_ERRNO,
	G_SYSFS_ERROR_UNKNOWN,
} GSysfsError;

#define G_SYSFS_ERROR g_sysfs_error_quark()

GQuark g_sysfs_error_quark(void);

gboolean g_sysfs_read_uint(GUdevDevice *device, const gchar *property,
			   guint *value, GError **error);
gboolean g_sysfs_write_uint(GUdevDevice *device, const gchar *property,
			    guint value, GError **error);

gchar *g_sysfs_read_string(GUdevDevice *device, const gchar *property,
			   gsize *length, GError **error);
gboolean g_sysfs_write_string(GUdevDevice *device, const gchar *property,
			      const gchar *string, GError **error);

G_END_DECLS

#endif
