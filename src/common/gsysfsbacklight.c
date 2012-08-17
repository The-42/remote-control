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

#define pr_fmt(fmt) "g-sysfs-backlight: " fmt

#include <string.h>
#include <gudev/gudev.h>
#include <linux/fb.h>

#include "gsysfsbacklight.h"
#include "glogging.h"

typedef struct {
	GUdevDevice *device;
	guint max_brightness;
} GSysfsBacklightPrivate;

#define G_SYSFS_BACKLIGHT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), G_SYSFS_TYPE_BACKLIGHT, GSysfsBacklightPrivate))

G_DEFINE_TYPE(GSysfsBacklight, g_sysfs_backlight, G_TYPE_OBJECT);

enum {
	PROP_0,
	PROP_DEVICE,
	PROP_MAX_BRIGHTNESS,
};

static void g_sysfs_backlight_set_device(GSysfsBacklight *self,
					 GUdevDevice *device)
{
	GSysfsBacklightPrivate *priv = G_SYSFS_BACKLIGHT_GET_PRIVATE(self);
	GError *error = NULL;
	guint value = 0;

	g_clear_object(&priv->device);

	priv->max_brightness = 0;
	priv->device = device;

	if (!g_sysfs_read_uint(device, "max_brightness", &value, &error)) {
		pr_debug("failed to read `max_brightness' property: %s",
			 error->message);
		g_clear_error(&error);
	}

	priv->max_brightness = value;
}

static void g_sysfs_backlight_get_property(GObject *object, guint prop_id,
					   GValue *value, GParamSpec *pspec)
{
	GSysfsBacklightPrivate *priv = G_SYSFS_BACKLIGHT_GET_PRIVATE(object);

	switch (prop_id) {
	case PROP_DEVICE:
		g_value_set_object(value, priv->device);
		break;

	case PROP_MAX_BRIGHTNESS:
		g_value_set_uint(value, priv->max_brightness);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void g_sysfs_backlight_set_property(GObject *object, guint prop_id,
					   const GValue *value,
					   GParamSpec *pspec)
{
	switch (prop_id) {
	case PROP_DEVICE:
		g_sysfs_backlight_set_device(G_SYSFS_BACKLIGHT(object),
					     g_value_dup_object(value));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void g_sysfs_backlight_finalize(GObject *object)
{
	GSysfsBacklightPrivate *priv = G_SYSFS_BACKLIGHT_GET_PRIVATE(object);

	g_object_unref(priv->device);
}

static void g_sysfs_backlight_class_init(GSysfsBacklightClass *klass)
{
	GObjectClass *object = G_OBJECT_CLASS(klass);

	object->get_property = g_sysfs_backlight_get_property;
	object->set_property = g_sysfs_backlight_set_property;
	object->finalize = g_sysfs_backlight_finalize;

	g_object_class_install_property(object, PROP_DEVICE,
			g_param_spec_object("device", "sysfs device",
					    "sysfs device",
					    G_UDEV_TYPE_DEVICE,
					    G_PARAM_READWRITE |
					    G_PARAM_CONSTRUCT |
					    G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(object, PROP_MAX_BRIGHTNESS,
			g_param_spec_uint("max-brightness",
					  "maximum brightness",
					  "maximum brightness",
					  0, G_MAXUINT32, 0,
					  G_PARAM_READABLE |
					  G_PARAM_STATIC_STRINGS));

	g_type_class_add_private(klass, sizeof(GSysfsBacklightPrivate));
}

static void g_sysfs_backlight_init(GSysfsBacklight *self)
{
}

GSysfsBacklight *g_sysfs_backlight_new(const gchar *name, GError **error)
{
	const gchar *const subsystems[] = { "backlight", NULL };
	GUdevDevice *device = NULL;
	GList *list, *entry;
	GUdevClient *udev;
	GObject *object;

	udev = g_udev_client_new(subsystems);
	if (!udev)
		return NULL;

	list = g_udev_client_query_by_subsystem(udev, "backlight");

	for (entry = list; entry != NULL; entry = g_list_next(entry)) {
		GUdevDevice *dev = G_UDEV_DEVICE(entry->data);
		const gchar *device_name;

		device_name = g_udev_device_get_name(dev);

		if (strcmp(device_name, name) == 0) {
			device = g_object_ref(dev);
			break;
		}
	}

	g_list_free_full(list, g_object_unref);
	g_object_unref(udev);

	object = g_object_new(G_SYSFS_TYPE_BACKLIGHT, "device", device, NULL);

	g_object_unref(device);

	return G_SYSFS_BACKLIGHT(object);
}

guint g_sysfs_backlight_get_max_brightness(GSysfsBacklight *self)
{
	GSysfsBacklightPrivate *priv = G_SYSFS_BACKLIGHT_GET_PRIVATE(self);

	return priv->max_brightness;
}

guint g_sysfs_backlight_get_brightness(GSysfsBacklight *self, GError **error)
{
	GSysfsBacklightPrivate *priv = G_SYSFS_BACKLIGHT_GET_PRIVATE(self);
	guint value;

	if (!g_sysfs_read_uint(priv->device, "brightness", &value, error))
		return 0;

	return value;
}

gboolean g_sysfs_backlight_set_brightness(GSysfsBacklight *self, guint value,
					  GError **error)
{
	GSysfsBacklightPrivate *priv = G_SYSFS_BACKLIGHT_GET_PRIVATE(self);

	return g_sysfs_write_uint(priv->device, "brightness", value, error);
}

gboolean g_sysfs_backlight_enable(GSysfsBacklight *self, GError **error)
{
	GSysfsBacklightPrivate *priv = G_SYSFS_BACKLIGHT_GET_PRIVATE(self);
	guint value = FB_BLANK_UNBLANK;

	return g_sysfs_write_uint(priv->device, "bl_power", value, error);
}

gboolean g_sysfs_backlight_disable(GSysfsBacklight *self, GError **error)
{
	GSysfsBacklightPrivate *priv = G_SYSFS_BACKLIGHT_GET_PRIVATE(self);
	guint value = FB_BLANK_POWERDOWN;

	return g_sysfs_write_uint(priv->device, "bl_power", value, error);
}
