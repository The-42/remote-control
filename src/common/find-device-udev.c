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

#include "find-device.h"

struct find_input_dev {
	device_found_cb callback;
	gpointer user;
};

static int on_input_device_found(gpointer user, GUdevDevice *device)
{
	struct find_input_dev *find = user;
	const gchar *filename = g_udev_device_get_device_file(device);
	if (filename) {
		int err = find->callback(find->user, filename);
		if (err < 0) {
			g_warning("probe: failed to use %s: %s",
				filename, g_strerror(-err));
		} else {
			g_debug("probe: added %s", filename);
		}
	}
	return 0;
}

gint find_input_devices(const gchar *devname, device_found_cb callback,
			gpointer user)
{
	struct find_input_dev find = {
		.callback = callback,
		.user = user,
	};
	struct udev_match matches[] = {
		{
			.type = UDEV_MATCH_SUBSYSTEM,
			.value = "input",
		},
		{
			.type = UDEV_MATCH_NAME,
			.value = "event*",
		},
		{
			.type = UDEV_MATCH_SYSFS_ATTR,
			.key = "../name",
			/* the strdup is needed because of the const */
			.value = g_strdup(devname),
		},
		{}
	};
	int err = find_udev_devices(
		matches, callback ? on_input_device_found : NULL, &find);
	g_free(matches[2].value);
	return err;
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

int parse_udev_match(const char *str, struct udev_match *match)
{
	static GRegex *regex = NULL;
	enum udev_match_type type;
	GMatchInfo *match_info;
	char *comp, *value;
	char *name, *key;

	if (regex == NULL) {
		regex = g_regex_new(
			"^([A-Z]+)({(.+)})? *([!=]=) *\"(.+)\" *$",
			0, 0, NULL);
		if (!regex) {
			g_warning("%s: failed to create regex", __func__);
			return -EINVAL;
		}
	}

	if (!g_regex_match_full(regex, str, -1, 0, 0, &match_info, NULL)) {
		g_warning("%s: udev match is invalid: %s", __func__, str);
		g_match_info_free (match_info);
		return -EINVAL;
	}

	name = g_match_info_fetch(match_info, 1);
	key = g_match_info_fetch(match_info, 3);
	comp = g_match_info_fetch(match_info, 4);
	value = g_match_info_fetch(match_info, 5);
	g_match_info_free (match_info);

	if (!name || !key || !comp || !value) {
		g_warning("%s: udev match is invalid: %s", __func__, str);
		goto on_error;
	}

	if (!g_strcmp0(name, "SUBSYSTEM"))
		type = UDEV_MATCH_SUBSYSTEM;
	else if (!g_strcmp0(name, "ATTR"))
		type = UDEV_MATCH_SYSFS_ATTR;
	else if (!g_strcmp0(name, "PROP"))
		type = UDEV_MATCH_PROPERTY;
	else if (!g_strcmp0(name, "KERNEL"))
		type = UDEV_MATCH_NAME;
	else if (!g_strcmp0(name, "TAG"))
		type = UDEV_MATCH_TAG;
	else {
		g_warning("%s: udev match is unkown: %s", __func__, name);
		goto on_error;
	}

	if ((type & UDEV_MATCH_HAS_KEY) && strlen(key) <= 0) {
		g_warning("%s: %s match needs a key", __func__, name);
		goto on_error;
	}
	if (!(type & UDEV_MATCH_HAS_KEY) && strlen(key) > 0) {
		g_warning("%s: %s match shouldn't have a key", __func__, name);
		goto on_error;
	}

	if (!g_strcmp0(comp, "!=")) {
		if (!(type & UDEV_MATCH_HAS_NOT)) {
			g_warning(
				"%s: %s match doesn't support negation",
				__func__, name);
			goto on_error;
		}
		type |= UDEV_MATCH_NOT;
	}

	match->type = type;
	match->key = key;
	match->value = value;

	g_free(name);
	g_free(comp);
	return 0;

on_error:
	g_free(name);
	g_free(key);
	g_free(comp);
	g_free(value);
	return -EINVAL;
}

int parse_udev_matches(char *const *list, struct udev_match **matchp)
{
	struct udev_match *match;
	int count, i, err;

	if (!list || !matchp)
		return -EINVAL;

	*matchp = NULL;

	for (count = 0; list[count]; count++)
		/* NOOP */;

	match = g_malloc0_n(count + 1, sizeof(*match));
	if (!match)
		return -ENOMEM;

	for (i = 0; i < count ; i++) {
		g_warning("%s: Parsing rule %s", __func__, list[i]);
		err = parse_udev_match(list[i], &match[i]);
		if (err) {
			free_udev_matches(match);
			return err;
		}
	}

	*matchp = match;
	return 0;
}

void uninit_udev_match(struct udev_match *match)
{
	if (!match)
		return;

	g_free(match->key);
	g_free(match->value);
	memset(match, 0, sizeof(*match));
}

void free_udev_match(struct udev_match *match)
{
	uninit_udev_match(match);
	g_free(match);
}

void free_udev_matches(struct udev_match *match)
{
	int i;

	if (!match)
		return;

	for (i = 0; match[i].type; i++)
		uninit_udev_match(&match[i]);
	g_free(match);
}
