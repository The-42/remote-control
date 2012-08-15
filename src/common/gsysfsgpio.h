/*
 * Copyright (C) 2012 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __G_SYSFS_GPIO_H__
#define __G_SYSFS_GPIO_H__

#include "gsysfs.h"

G_BEGIN_DECLS

#define G_SYSFS_TYPE_GPIO		(g_sysfs_gpio_get_type())
#define G_SYSFS_GPIO(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), G_SYSFS_TYPE_GPIO, GSysfsGpio))
#define G_SYSFS_IS_GPIO(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), G_SYSFS_TYPE_GPIO))
#define G_SYSFS_GPIO_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass), G_SYSFS_TYPE_GPIO, GSysfsGpioClass))
#define G_SYSFS_IS_GPIO_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), G_SYSFS_TYPE_GPIO))
#define G_SYSFS_GPIO_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), G_SYSFS_TYPE_GPIO, GSysfsGpioClass))

typedef struct _GSysfsGpio GSysfsGpio;
typedef struct _GSysfsGpioClass GSysfsGpioClass;

struct _GSysfsGpio {
	GObject parent;
};

struct _GSysfsGpioClass {
	GObjectClass parent;
};

GType g_sysfs_gpio_get_type(void);

typedef enum {
	G_SYSFS_GPIO_DIRECTION_INPUT,
	G_SYSFS_GPIO_DIRECTION_OUTPUT,
	G_SYSFS_GPIO_DIRECTION_HIGH,
	G_SYSFS_GPIO_DIRECTION_LOW,
} GSysfsGpioDirection;

#define G_SYSFS_TYPE_GPIO_DIRECTION g_sysfs_gpio_direction_get_type()
GType g_sysfs_gpio_direction_get_type(void);

typedef enum {
	G_SYSFS_GPIO_FLAGS_INPUT = 0 << 0,
	G_SYSFS_GPIO_FLAGS_OUTPUT = 1 << 0,
	G_SYSFS_GPIO_FLAGS_OUTPUT_LOW = 0 << 1,
	G_SYSFS_GPIO_FLAGS_OUTPUT_HIGH = 1 << 1,
} GSysfsGpioFlags;

#define G_SYSFS_TYPE_GPIO_FLAGS g_sysfs_gpio_flags_get_type()
GType g_sysfs_gpio_flags_get_type(void);

GSysfsGpio *g_sysfs_gpio_new(const gchar *chip, guint pin, guint flags,
			     GError **error);
GSysfsGpio *g_key_file_get_gpio(GKeyFile *keyfile, const gchar *group,
				const gchar *key, GError **error);
gboolean g_sysfs_gpio_set_direction(GSysfsGpio *gpio,
				    GSysfsGpioDirection direction,
				    guint value, GError **error);
GSysfsGpioDirection g_sysfs_gpio_get_direction(GSysfsGpio *gpio,
					       GError **error);
gboolean g_sysfs_gpio_set_value(GSysfsGpio *gpio, guint value,
				GError **error);
guint g_sysfs_gpio_get_value(GSysfsGpio *gpio, GError **error);

G_END_DECLS

#endif
