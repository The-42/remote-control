/*
 * Copyright (C) 2012 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __G_SYSFS_BACKLIGHT_H__
#define __G_SYSFS_BACKLIGHT_H__

#include "gsysfs.h"

G_BEGIN_DECLS

#define G_SYSFS_TYPE_BACKLIGHT			(g_sysfs_backlight_get_type())
#define G_SYSFS_BACKLIGHT(obj)			(G_TYPE_CHECK_INSTANCE_CAST((obj), G_SYSFS_TYPE_BACKLIGHT, GSysfsBacklight))
#define G_SYSFS_IS_BACKLIGHT(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), G_SYSFS_TYPE_BACKLIGHT))
#define G_SYSFS_BACKLIGHT_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), G_SYSFS_TYPE_BACKLIGHT, GSysfsBacklightClass))
#define G_SYSFS_IS_BACKLIGHT_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), G_SYSFS_TYPE_BACKLIGHT))
#define G_SYSFS_BACKLIGHT_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), G_SYSFS_TYPE_BACKLIGHT, GSysfsBacklightClass))

typedef struct _GSysfsBacklight GSysfsBacklight;
typedef struct _GSysfsBacklightClass GSysfsBacklightClass;

struct _GSysfsBacklight {
	GObject parent;
};

struct _GSysfsBacklightClass {
	GObjectClass parent;
};

GType g_sysfs_backlight_get_type(void);

GSysfsBacklight *g_sysfs_backlight_new(const gchar *name, GError **error);

guint g_sysfs_backlight_get_max_brightness(GSysfsBacklight *backlight);
guint g_sysfs_backlight_get_brightness(GSysfsBacklight *backlight,
				       GError **error);
gboolean g_sysfs_backlight_set_brightness(GSysfsBacklight *backlight,
					  guint brightness, GError **error);
gboolean g_sysfs_backlight_enable(GSysfsBacklight *backlight, GError **error);
gboolean g_sysfs_backlight_disable(GSysfsBacklight *backlight,
				   GError **error);
gboolean g_sysfs_backlight_is_enabled(GSysfsBacklight *backlight,
				      GError **error);

G_END_DECLS

#endif
