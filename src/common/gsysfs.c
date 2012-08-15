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
#include <stdio.h>
#include <string.h>

#include "gsysfs.h"

GQuark g_sysfs_error_quark(void)
{
	return g_quark_from_static_string("g-sysfs-error-quark");
}

gboolean g_sysfs_read_uint(GUdevDevice *device, const gchar *property,
			   guint *value, GError **error)
{
	const gchar *path;
	gchar *filename;
	FILE *fp;
	int err;

	path = g_udev_device_get_sysfs_path(device);

	filename = g_strdup_printf("%s/%s", path, property);
	if (!filename) {
		g_set_error(error, G_SYSFS_ERROR, G_SYSFS_ERROR_OUT_OF_MEMORY,
			    "out of memory");
		return FALSE;
	}

	fp = fopen(filename, "r");
	if (!fp) {
		g_set_error(error, G_SYSFS_ERROR, G_SYSFS_ERROR_ERRNO,
			    "%s", g_strerror(errno));
		g_free(filename);
		return FALSE;
	}

	g_free(filename);

	err = fscanf(fp, "%u", value);
	if (err < 0) {
		g_set_error(error, G_SYSFS_ERROR, G_SYSFS_ERROR_ERRNO,
			    "%s", g_strerror(errno));
		fclose(fp);
		return FALSE;
	}

	fclose(fp);

	if (err != 1) {
		g_set_error(error, G_SYSFS_ERROR, G_SYSFS_ERROR_PARSE,
			    "cannot parse `%s' property", property);
		return FALSE;
	}

	return TRUE;
}

gboolean g_sysfs_write_uint(GUdevDevice *device, const gchar *property,
			    guint value, GError **error)
{
	const gchar *path;
	gchar *filename;
	FILE *fp;
	int err;

	path = g_udev_device_get_sysfs_path(device);

	filename = g_strdup_printf("%s/%s", path, property);
	if (!filename) {
		g_set_error(error, G_SYSFS_ERROR, G_SYSFS_ERROR_OUT_OF_MEMORY,
			    "out of memory");
		return FALSE;
	}

	fp = fopen(filename, "w");
	if (!fp) {
		g_set_error(error, G_SYSFS_ERROR, G_SYSFS_ERROR_ERRNO,
			    "%s", g_strerror(errno));
		g_free(filename);
		return FALSE;
	}

	g_free(filename);

	err = fprintf(fp, "%u", value);
	if (err <= 0) {
		g_set_error(error, G_SYSFS_ERROR, G_SYSFS_ERROR_ERRNO,
			    "%s", g_strerror(errno));
		fclose(fp);
		return FALSE;
	}

	fclose(fp);

	return TRUE;
}

gchar *g_sysfs_read_string(GUdevDevice *device, const gchar *property,
			   gsize *length, GError **error)
{
	const gchar *path;
	gchar string[80];
	gchar *filename;
	FILE *fp;

	path = g_udev_device_get_sysfs_path(device);
	if (!path) {
		g_set_error(error, G_SYSFS_ERROR, G_SYSFS_ERROR_UNKNOWN,
			    "sysfs path not available for `%s'",
			    g_udev_device_get_name(device));
		return NULL;
	}

	filename = g_strdup_printf("%s/%s", path, property);
	if (!filename) {
		g_set_error(error, G_SYSFS_ERROR, G_SYSFS_ERROR_OUT_OF_MEMORY,
			    "out of memory");
		return NULL;
	}

	fp = fopen(filename, "r");
	if (!fp) {
		g_set_error(error, G_SYSFS_ERROR, G_SYSFS_ERROR_ERRNO,
			    "%s", g_strerror(errno));
		g_free(filename);
		return NULL;
	}

	g_free(filename);

	if (!fgets(string, sizeof(string), fp)) {
		g_set_error(error, G_SYSFS_ERROR, G_SYSFS_ERROR_ERRNO,
			    "%s", g_strerror(errno));
		fclose(fp);
		return NULL;
	}

	fclose(fp);

	if (length)
		*length = strlen(string);

	return g_strdup(string);
}

gboolean g_sysfs_write_string(GUdevDevice *device, const gchar *property,
			      const gchar *string, GError **error)
{
	const gchar *path;
	gchar *filename;
	FILE *fp;
	int err;

	path = g_udev_device_get_sysfs_path(device);
	if (!path) {
		g_set_error(error, G_SYSFS_ERROR, G_SYSFS_ERROR_UNKNOWN,
			    "sysfs path not available for `%s'",
			    g_udev_device_get_name(device));
		return FALSE;
	}

	filename = g_strdup_printf("%s/%s", path, property);
	if (!filename) {
		g_set_error(error, G_SYSFS_ERROR, G_SYSFS_ERROR_OUT_OF_MEMORY,
			    "out of memory");
		return FALSE;
	}

	fp = fopen(filename, "w");
	if (!fp) {
		g_set_error(error, G_SYSFS_ERROR, G_SYSFS_ERROR_ERRNO,
			    "%s", g_strerror(errno));
		g_free(filename);
		return FALSE;
	}

	g_free(filename);

	err = fprintf(fp, "%s", string);
	if (err < 0) {
		g_set_error(error, G_SYSFS_ERROR, G_SYSFS_ERROR_ERRNO,
			    "%s", g_strerror(errno));
		fclose(fp);
		return FALSE;
	}

	fclose(fp);

	return TRUE;
}
