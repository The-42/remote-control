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

#define pr_fmt(fmt) "g-sysfs-gpio: " fmt

#include <gpio.h>
#include <stdlib.h>
#include <string.h>

#include "gsysfsgpio.h"
#include "glogging.h"

typedef struct {
	GUdevDevice *device;
	guint pin;
} GSysfsGpioPrivate;

#define G_SYSFS_GPIO_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), G_SYSFS_TYPE_GPIO, GSysfsGpioPrivate))

G_DEFINE_TYPE(GSysfsGpio, g_sysfs_gpio, G_TYPE_OBJECT);

enum {
	PROP_0,
	PROP_PIN,
	PROP_DEVICE,
	PROP_DIRECTION,
	PROP_LEVEL,
};

static gboolean gpio_get_direction(GSysfsGpio *gpio,
				   GSysfsGpioDirection *direction,
				   GError **errorp)
{
	GSysfsGpioPrivate *priv = G_SYSFS_GPIO_GET_PRIVATE(gpio);
	GError *error = NULL;
	gboolean ret = TRUE;
	gsize length;
	gchar *dir;

	dir = g_sysfs_read_string(priv->device, "direction", &length, &error);
	if (!dir) {
		g_propagate_error(errorp, error);
		return FALSE;
	}

	if (strcmp(dir, "out") == 0)
		*direction = G_SYSFS_GPIO_DIRECTION_OUTPUT;
	else if (strcmp(dir, "in") == 0)
		*direction = G_SYSFS_GPIO_DIRECTION_INPUT;
	else {
		g_set_error(errorp, G_SYSFS_ERROR, G_SYSFS_ERROR_INVALID_VALUE,
			    "invalid value for `direction': %s", dir);
		ret = FALSE;
	}

	g_free(dir);
	return ret;
}

static gboolean gpio_set_direction(GSysfsGpio *gpio,
				   GSysfsGpioDirection direction,
				   GError **errorp)
{
	GSysfsGpioPrivate *priv = G_SYSFS_GPIO_GET_PRIVATE(gpio);
	const gchar *dir;

	switch (direction) {
	case G_SYSFS_GPIO_DIRECTION_INPUT:
		dir = "in";
		break;

	case G_SYSFS_GPIO_DIRECTION_HIGH:
		dir = "high";
		break;

	case G_SYSFS_GPIO_DIRECTION_LOW:
		dir = "low";
		break;

	default:
		g_set_error(errorp, G_SYSFS_ERROR, G_SYSFS_ERROR_INVALID_VALUE,
			    "invalid value for `direction'");
		return FALSE;
	}

	return g_sysfs_write_string(priv->device, "direction", dir, errorp);
}

static gboolean gpio_get_level(GSysfsGpio *gpio, guint *level, GError **error)
{
	GSysfsGpioPrivate *priv = G_SYSFS_GPIO_GET_PRIVATE(gpio);

	return g_sysfs_read_uint(priv->device, "value", level, error);
}

static gboolean gpio_set_level(GSysfsGpio *gpio, guint level, GError **error)
{
	GSysfsGpioPrivate *priv = G_SYSFS_GPIO_GET_PRIVATE(gpio);

	return g_sysfs_write_uint(priv->device, "value", level, error);
}

static void g_sysfs_gpio_get_property(GObject *object, guint prop_id,
				      GValue *value, GParamSpec *pspec)
{
	GSysfsGpioPrivate *priv = G_SYSFS_GPIO_GET_PRIVATE(object);
	GSysfsGpio *gpio = G_SYSFS_GPIO(object);
	GSysfsGpioDirection direction;
	GError *error = NULL;
	guint level;

	switch (prop_id) {
	case PROP_PIN:
		g_value_set_uint(value, priv->pin);
		break;

	case PROP_DEVICE:
		g_value_set_object(value, priv->device);
		break;

	case PROP_DIRECTION:
		if (gpio_get_direction(gpio, &direction, &error)) {
			g_value_set_enum(value, direction);
		} else {
			pr_debug("failed to get direction: %s",
				 error->message);
			g_clear_error(&error);
		}
		break;

	case PROP_LEVEL:
		if (gpio_get_level(gpio, &level, &error)) {
			g_value_set_uint(value, level);
		} else {
			pr_debug("failed to get pin level: %s",
				 error->message);
			g_clear_error(&error);
		}
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void g_sysfs_gpio_set_property(GObject *object, guint prop_id,
				      const GValue *value, GParamSpec *pspec)
{
	GSysfsGpioPrivate *priv = G_SYSFS_GPIO_GET_PRIVATE(object);
	GSysfsGpio *gpio = G_SYSFS_GPIO(object);
	GSysfsGpioDirection direction;
	GError *error = NULL;
	guint level;

	switch (prop_id) {
	case PROP_PIN:
		priv->pin = g_value_get_uint(value);
		break;

	case PROP_DEVICE:
		g_clear_object(&priv->device);
		priv->device = g_value_dup_object(value);
		break;

	case PROP_DIRECTION:
		direction = g_value_get_enum(value);

		if (!gpio_set_direction(gpio, direction, &error)) {
			pr_debug("failed to set direction: %s",
				 error->message);
			g_clear_error(&error);
		}
		break;

	case PROP_LEVEL:
		level = g_value_get_uint(value);

		if (!gpio_set_level(gpio, level, &error)) {
			pr_debug("failed to set pin level: %s",
				 error->message);
			g_clear_error(&error);
		}
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void g_sysfs_gpio_finalize(GObject *object)
{
	GSysfsGpioPrivate *priv = G_SYSFS_GPIO_GET_PRIVATE(object);
	const gchar *const subsystems[] = { "gpio", NULL };
	GError *error = NULL;
	GUdevDevice *subsys;
	GUdevClient *udev;

	udev = g_udev_client_new(subsystems);
	if (!udev) {
		pr_debug("Udev not found");
		goto out;
	}

	subsys = g_udev_client_query_by_sysfs_path(udev, "/sys/class/gpio");
	if (!subsys) {
		pr_debug("GPIO subsystem not found");
		g_object_unref(udev);
		goto out;
	}

	g_object_unref(udev);

	if (!g_sysfs_write_uint(subsys, "unexport", priv->pin, &error)) {
		pr_debug("failed to unexport GPIO#%u: %s", priv->pin,
			 error->message);
		g_clear_error(&error);
	}

	g_object_unref(subsys);

out:
	g_object_ref(priv->device);
}

static void g_sysfs_gpio_class_init(GSysfsGpioClass *klass)
{
	GObjectClass *object = G_OBJECT_CLASS(klass);

	object->get_property = g_sysfs_gpio_get_property;
	object->set_property = g_sysfs_gpio_set_property;
	object->finalize = g_sysfs_gpio_finalize;

	g_object_class_install_property(object, PROP_PIN,
			g_param_spec_uint("pin", "pin number",
					  "pin number",
					  0, G_MAXUINT32, 0,
					  G_PARAM_READWRITE |
					  G_PARAM_CONSTRUCT_ONLY |
					  G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(object, PROP_DEVICE,
			g_param_spec_object("device", "sysfs device",
					    "sysfs device",
					    G_UDEV_TYPE_DEVICE,
					    G_PARAM_READWRITE |
					    G_PARAM_CONSTRUCT_ONLY |
					    G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(object, PROP_DIRECTION,
			g_param_spec_enum("direction", "GPIO direction",
					  "GPIO direction",
					  G_SYSFS_TYPE_GPIO_DIRECTION,
					  G_SYSFS_GPIO_DIRECTION_INPUT,
					  G_PARAM_READWRITE |
					  G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(object, PROP_LEVEL,
			g_param_spec_uint("level", "GPIO pin level",
					  "GPIO pin level",
					  0, 1, 0,
					  G_PARAM_READWRITE |
					  G_PARAM_STATIC_STRINGS));

	g_type_class_add_private(klass, sizeof(GSysfsGpioPrivate));
}

static void g_sysfs_gpio_init(GSysfsGpio *self)
{
}

GType g_sysfs_gpio_direction_get_type(void)
{
	static GEnumValue values[] = {
		{ G_SYSFS_GPIO_DIRECTION_INPUT,  "input",  "input"  },
		{ G_SYSFS_GPIO_DIRECTION_OUTPUT, "output", "output" },
		{ G_SYSFS_GPIO_DIRECTION_HIGH,   "high",   "high"   },
		{ G_SYSFS_GPIO_DIRECTION_LOW,    "low",    "low"    },
		{ 0, NULL, NULL }
	};

	return g_enum_register_static("g-sysfs-gpio-direction", values);
}

GType g_sysfs_gpio_flags_get_type(void)
{
	static GFlagsValue values[] = {
		{ G_SYSFS_GPIO_FLAGS_INPUT,       "input",  "in"   },
		{ G_SYSFS_GPIO_FLAGS_OUTPUT,      "output", "out"  },
		{ G_SYSFS_GPIO_FLAGS_OUTPUT_LOW,  "low",    "low"  },
		{ G_SYSFS_GPIO_FLAGS_OUTPUT_HIGH, "high",   "high" },
		{ 0, NULL, NULL }
	};

	return g_flags_register_static("g-sysfs-gpio-flags", values);
}

GSysfsGpio *g_sysfs_gpio_new(const gchar *chip, guint pin, guint flags,
			     GError **errorp)
{
	const gchar *const subsystems[] = { "gpio", NULL };
	GSysfsGpioDirection direction;
	GError *error = NULL;
	GUdevDevice *device;
	GUdevClient *udev;
	GSysfsGpio *gpio;
	gchar *name;
	guint base;

	udev = g_udev_client_new(subsystems);
	if (!udev) {
		g_set_error(errorp, G_SYSFS_ERROR,
			    G_SYSFS_ERROR_DEVICE_NOT_FOUND,
			    "Udev not found");
		return NULL;
	}

	device = g_udev_client_query_by_subsystem_and_name(udev, "gpio", chip);
	if (!device) {
		g_set_error(errorp, G_SYSFS_ERROR,
			    G_SYSFS_ERROR_DEVICE_NOT_FOUND,
			    "GPIO chip `%s' not found", chip);
		g_object_unref(udev);
		return NULL;
	}

	if (!g_sysfs_read_uint(device, "base", &base, &error)) {
		g_propagate_error(errorp, error);
		g_object_unref(device);
		g_object_unref(udev);
		return NULL;
	}

	g_object_unref(device);

	device = g_udev_client_query_by_sysfs_path(udev, "/sys/class/gpio");
	if (!device) {
		g_set_error(errorp, G_SYSFS_ERROR,
			    G_SYSFS_ERROR_DEVICE_NOT_FOUND,
			    "`/dev/class/gpio' not found");
		g_object_unref(udev);
		return NULL;
	}

	if (!g_sysfs_write_uint(device, "export", base + pin, &error)) {
		g_propagate_error(errorp, error);
		g_object_unref(device);
		g_object_unref(udev);
		return NULL;
	}

	g_object_unref(device);

	name = g_strdup_printf("gpio%u", base + pin);
	if (!name) {
		g_set_error(errorp, G_SYSFS_ERROR, G_SYSFS_ERROR_OUT_OF_MEMORY,
			    "out of memory");
		g_object_unref(udev);
		return NULL;
	}

	device = g_udev_client_query_by_subsystem_and_name(udev, "gpio", name);
	if (!device) {
		g_set_error(errorp, G_SYSFS_ERROR,
			    G_SYSFS_ERROR_DEVICE_NOT_FOUND,
			    "GPIO#%u not found", base + pin);
		g_free(name);
		g_object_unref(udev);
		return NULL;
	}

	g_object_unref(udev);
	g_free(name);

	if (flags & G_SYSFS_GPIO_FLAGS_OUTPUT) {
		if (flags & G_SYSFS_GPIO_FLAGS_OUTPUT_HIGH)
			direction = G_SYSFS_GPIO_DIRECTION_HIGH;
		else
			direction = G_SYSFS_GPIO_DIRECTION_LOW;
	} else {
		direction = G_SYSFS_GPIO_DIRECTION_INPUT;
	}

	gpio = g_object_new(G_SYSFS_TYPE_GPIO, "pin", base + pin, "device",
			    device, "direction", direction, NULL);

	return gpio;
}

GSysfsGpio *g_key_file_get_gpio(GKeyFile *keyfile, const gchar *group,
				const gchar *key, GError **error)
{
	unsigned long pin, flags;
	gchar *value, **parts;
	GSysfsGpio *gpio;
	char *end;

	value = g_key_file_get_value(keyfile, group, key, error);
	if (!value)
		return NULL;

	parts = g_strsplit(value, ":", 3);
	if (!parts) {
		g_set_error(error, G_SYSFS_ERROR, G_SYSFS_ERROR_PARSE,
			    "failed to parse value `%s'", value);
		g_free(value);
		return NULL;
	}

	g_free(value);

	pin = strtoul(parts[1], &end, 0);
	if (end == parts[1]) {
		g_set_error(error, G_SYSFS_ERROR,
			    G_SYSFS_ERROR_INVALID_VALUE,
			    "invalid GPIO pin number: %s", parts[1]);
		goto freev;
	}

	flags = strtoul(parts[2], &end, 0);
	if (end == parts[2]) {
		g_set_error(error, G_SYSFS_ERROR, G_SYSFS_ERROR_INVALID_VALUE,
			    "invalid flags value: %s", parts[2]);
		goto freev;
	}

	gpio = g_sysfs_gpio_new(parts[0], pin, flags, error);

	g_strfreev(parts);

	return gpio;

freev:
	g_strfreev(parts);
	return NULL;
}

gboolean g_sysfs_gpio_set_value(GSysfsGpio *gpio, guint value, GError **error)
{
	GSysfsGpioPrivate *priv = G_SYSFS_GPIO_GET_PRIVATE(gpio);

	return g_sysfs_write_uint(priv->device, "value", value, error);
}

guint g_sysfs_gpio_get_value(GSysfsGpio *gpio, GError **error)
{
	GSysfsGpioPrivate *priv = G_SYSFS_GPIO_GET_PRIVATE(gpio);
	guint value;

	if (!g_sysfs_read_uint(priv->device, "value", &value, error))
		return 0;

	return value;
}
