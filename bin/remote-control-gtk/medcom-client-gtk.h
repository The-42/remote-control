/*
 * Copyright (C) 2010-2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef RCC_GTK_H
#define RCC_GTK_H 1

#include <errno.h>
#include <stdint.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

#include "remote-control.h"

extern struct medcom_client *g_client;

struct panel {
	const char *name;
	void *priv;

	int (*create)(struct panel *panel, GtkWidget **widget);
	int (*close)(struct panel *panel);
	int (*destroy)(struct panel *panel);
};

static inline int panel_create(struct panel *panel, GtkWidget **widget)
{
	if (panel && panel->create)
		return panel->create(panel, widget);

	return panel ? -ENOSYS : -EINVAL;
}

static inline int panel_destroy(struct panel *panel)
{
	if (panel && panel->destroy)
		return panel->destroy(panel);

	return panel ? -ENOSYS : -EINVAL;
}

#endif /* !RCC_GTK_H */
