/*
 * Copyright (C) 2010-2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "medcom-client-gtk.h"

static int miscellaneous_panel_create(struct panel *panel, GtkWidget **widget)
{
	return -ENOSYS;
}

struct panel miscellaneous_panel = {
	.name = "Miscellaneous",
	.create = miscellaneous_panel_create,
	.destroy = NULL,
};
