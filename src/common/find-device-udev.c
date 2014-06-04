/*
 * Copyright (C) 2013 Avionic Design GmbH
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
#include <glib.h>

#include <linux/input.h>

#include "find-device.h"


static inline void free_list_item(gpointer data)
{
	GUdevDevice *device = data;
	g_object_unref(device);
}

gint find_input_devices(const gchar *devname, device_found_cb callback,
			gpointer user)
{
	const gchar * const subsystems[] = { "input", NULL };
	GUdevEnumerator *enumerate;
	GUdevClient *client;
	GPatternSpec *event;
	GList *devices, *node;
	gint found = 0;

	client = g_udev_client_new(subsystems);
	if (!client) {
		g_debug("probe: failed to create UDEV client");
		return -ENOMEM;
	}

	enumerate = g_udev_enumerator_new(client);
	if (!enumerate) {
		g_debug("probe: failed to create enumerator");
		g_object_unref(client);
		return -ENOMEM;
	}

	g_udev_enumerator_add_match_subsystem(enumerate, "input");

	event = g_pattern_spec_new("event*");
	devices = g_udev_enumerator_execute(enumerate);

	for (node = g_list_first(devices); node; node = node->next) {
		GUdevDevice *device = node->data;
		GUdevDevice *parent;
		const gchar *name;

		name = g_udev_device_get_name(device);
		if (!name || !g_pattern_match_string(event, name))
			continue;

		parent = g_udev_device_get_parent(device);
		name = g_udev_device_get_sysfs_attr(parent, "name");

		if (g_str_equal(name, devname)) {
			const gchar *filename;
			g_debug("probe: using device:  %s", name);
			filename = g_udev_device_get_device_file(device);
			if (filename) {
				if (callback) {
					int err = callback(user, filename);
					if (err < 0) {
						g_warning("probe: failed to use %s: %s",
							  filename, g_strerror(-err));
					} else {
						g_debug("probe: added %s", filename);
					}
				}
				found++;
			}
		}
		g_object_unref(parent);
	}

	g_list_free_full(devices, free_list_item);
	g_pattern_spec_free(event);
	g_object_unref(enumerate);
	g_object_unref(client);

	return found;
}

gint find_udev_devices(const struct udev_match *match,
		udev_device_found_cb callback, gpointer user)
{
	GUdevEnumerator *enumerator;
	GList *devices, *node;
	GUdevClient *client;
	gint found = 0;
	int i;

	if (!match)
		return -EINVAL;

	client = g_udev_client_new(NULL);
	if (!client) {
		g_debug("%s: failed to create UDEV client", __func__);
		return -ENOMEM;
	}

	enumerator = g_udev_enumerator_new(client);
	if (!enumerator) {
		g_debug("%s: failed to create enumerator", __func__);
		g_object_unref(client);
		return -ENOMEM;
	}

	for (i = 0; match->type; i++, match++) {
		if (!match->value) {
			g_warning("%s: match %d is missing a value",
				__func__, i);
			continue;
		}
		if ((match->type & UDEV_MATCH_HAS_KEY) && !match->key) {
			g_warning("%s: match %d is missing a key",
				__func__, i);
			continue;
		}
		switch (match->type) {
		case UDEV_MATCH_SUBSYSTEM:
			g_udev_enumerator_add_match_subsystem(
				enumerator, match->value);
			break;
		case UDEV_MATCH_NOT_SUBSYSTEM:
			g_udev_enumerator_add_nomatch_subsystem(
				enumerator, match->value);
			break;
		case UDEV_MATCH_SYSFS_ATTR:
			g_udev_enumerator_add_match_sysfs_attr(
				enumerator, match->key, match->value);
			break;
		case UDEV_MATCH_NOT_SYSFS_ATTR:
			g_udev_enumerator_add_nomatch_sysfs_attr(
				enumerator, match->key, match->value);
			break;
		case UDEV_MATCH_PROPERTY:
			g_udev_enumerator_add_match_property(
				enumerator, match->key, match->value);
			break;
		case UDEV_MATCH_NAME:
			g_udev_enumerator_add_match_name(
				enumerator, match->value);
			break;
		case UDEV_MATCH_TAG:
			g_udev_enumerator_add_match_tag(
				enumerator, match->value);
			break;
		default:
			g_warning("%s: match %d has an unknown type %d",
				__func__, i, match->type);
		}
	}

	devices = g_udev_enumerator_execute(enumerator);

	for (node = g_list_first(devices); node; node = node->next) {
		GUdevDevice *device = node->data;
		if (callback) {
			if (callback(user, device) < 0)
				callback = NULL;
		}
		found += 1;
		g_object_unref(device);
	}

	g_list_free(devices);
	g_object_unref(enumerator);
	g_object_unref(client);

	return found;
}
