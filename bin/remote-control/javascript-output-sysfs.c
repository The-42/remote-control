/*
 * Copyright (C) 2014 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <errno.h>
#include <math.h>

#include "find-device.h"
#include "javascript.h"
#include "javascript-output.h"

struct js_output {
	char *path;
	struct udev_match *udev_match;
	char *attr;
	double min, max;
};

static int js_output_sysfs_set(struct js_output *out, double value)
{
	int ival, err;
	FILE *fd;

	if (!out->path)
		return -ENODEV;

	fd = fopen(out->path, "w");
	if (fd == NULL)
		return -errno;

	if (value < out->min)
		ival = out->min;
	else if (value > out->max)
		ival = out->max;
	else
		ival = value;

	err = fprintf(fd, "%d", ival);
	fclose(fd);

	return err < 0 ? -errno : 0;
}

static int js_output_sysfs_get(struct js_output *out, double *valuep)
{
	int ival, err;
	FILE *fd;

	if (!out->path)
		return -ENODEV;

	fd = fopen(out->path, "r");
	if (fd == NULL)
		return -errno;

	err = fscanf(fd, "%d", &ival);
	fclose(fd);

	if (err > 0) {
		*valuep = ival;
		return 0;
	}
	return err < 0 ? -errno : -EIO;
}

static int on_output_device_found(gpointer user, GUdevDevice *dev)
{
	struct js_output *out = user;
	const char *base;

	base = g_udev_device_get_sysfs_path(dev);
	if (!base)
		return 0;

	out->path = g_strdup_printf("%s/%s", base, out->attr);
	return -1; /* stop searching */
}

static int js_output_sysfs_prepare(struct js_output *out)
{
	int err;

	if (!out->udev_match || !out->attr)
		return 0;

	if (out->path) {
		g_free(out->path);
		out->path = NULL;
	}

	err = find_udev_devices(out->udev_match, on_output_device_found, out);
	return err == 0 ? -ENOENT : err;
}

static int js_output_sysfs_create(
	GKeyFile *config, const char* name, struct js_output **outp)
{
	struct js_output *output;
	char *path = NULL;
	char *attr = NULL;
	char **match = NULL;
	struct udev_match *udev_match;
	double min, max;
	int err;

	if (!config || !name || !outp)
		return -EINVAL;

	match = javascript_config_get_string_list(config, OUTPUT_GROUP, name,
			"match");
	if (match) {
		attr = javascript_config_get_string(config, OUTPUT_GROUP, name,
				"attr");
		if (!attr) {
			g_error("%s: Output %s doesn't have an attribute",
				__func__, name);
			err = -EINVAL;
			goto on_error;
		}
		err = parse_udev_matches(match, &udev_match);
		if (err) {
			g_error("%s: Failed to parse match rule of output %s",
				__func__, name);
			goto on_error;
		}
		g_strfreev(match);
		match = NULL;
	} else {
		path = javascript_config_get_string(config, OUTPUT_GROUP, name,
				"path");
		if (!path) {
			g_error("%s: Output %s doesn't have a path or match rule",
				__func__, name);
			err = -EINVAL;
			goto on_error;
		}
	}

	min = javascript_config_get_double(config, OUTPUT_GROUP, name, "min");
	if (isnan(min))
		min = 0.0;

	max = javascript_config_get_double(config, OUTPUT_GROUP, name, "max");
	if (isnan(max))
		max = 1.0;

	output = g_malloc0(sizeof(*output));
	if (!output) {
		err = -ENOMEM;
		goto on_error;
	}

	output->path = path;
	output->udev_match = udev_match;
	output->attr = attr;
	output->min = min;
	output->max = max;

	*outp = output;
	return 0;

on_error:
	g_free(path);
	g_strfreev(match);
	g_free(attr);
	return err;
}

const struct js_output_type js_output_sysfs = {
	.name = "sysfs",
	.create = js_output_sysfs_create,
	.prepare = js_output_sysfs_prepare,
	.set = js_output_sysfs_set,
	.get = js_output_sysfs_get,
};
