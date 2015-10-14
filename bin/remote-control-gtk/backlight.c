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

void on_brightness_value_changed(GtkWidget *widget, gpointer data)
{
	gdouble value = gtk_range_get_value(GTK_RANGE(widget));
	uint8_t brightness = (uint8_t)value;
	remote_backlight_set(g_client, brightness);
}

void on_backlight_enable_toggled(GtkWidget *widget, gpointer data)
{
	gboolean enabled;
	int32_t ret = 0;

	enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

	ret = remote_backlight_enable(g_client, enabled);
	if (ret < 0) {
		fprintf(stderr, "failed to %s backlight: %d\n",
				enabled ? "enable" : "disable", ret);
	}
}

static int backlight_panel_create(struct panel *panel, GtkWidget **widget)
{
	GtkWidget *container;
	GtkWidget *control;
	GladeXML *xml;

	if (!panel)
		return -EINVAL;

	xml = glade_xml_new(GLADE_FILE, "backlight_panel", NULL);
	glade_xml_signal_autoconnect(xml);

	container = glade_xml_get_widget(xml, "backlight_panel");
	if (!container) {
		fprintf(stderr, "failed to get backlight panel\n");
		return -ENOENT;
	}

	control = glade_xml_get_widget(xml, "brightness");
	if (control) {
		uint8_t brightness = 255;
		remote_backlight_get(g_client, &brightness);
		gtk_range_set_value(GTK_RANGE(control), brightness);
	}

	control = glade_xml_get_widget(xml, "backlight");
	if (control) {
	}

	gtk_widget_show(container);
	*widget = container;

	return 0;
}

struct panel backlight_panel = {
	.name = "Backlight",
	.create = backlight_panel_create,
	.destroy = NULL,
};
