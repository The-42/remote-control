/*
 * Copyright (C) 2010-2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "remote-control-gtk.h"

static int tuner_panel_create(struct panel *panel, GtkWidget **widget)
{
	GtkWidget *container;
	GladeXML *xml;

	if (!panel)
		return -EINVAL;

	xml = glade_xml_new("remote-client-gtk.glade", "tuner_panel", NULL);
	glade_xml_signal_autoconnect(xml);

	container = glade_xml_get_widget(xml, "tuner_panel");
	if (!container) {
		fprintf(stderr, "failed to get tuner panel\n");
		return -ENOENT;
	}

	gtk_widget_show(container);
	*widget = container;

	return 0;
}

struct panel tuner_panel = {
	.name = "Tuner",
	.create = tuner_panel_create,
	.destroy = NULL,
};
