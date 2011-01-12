/*
 * Copyright (C) 2010-2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "remote-control-gtk.h"

static int modem_panel_create(struct panel *panel, GtkWidget **widget)
{
	return -ENOSYS;
}

struct panel modem_panel = {
	.name = "Modem",
	.create = modem_panel_create,
	.destroy = NULL,
};
