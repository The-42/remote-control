#include "medcom-client-gtk.h"

static int tuner_panel_create(struct panel *panel, GtkWidget **widget)
{
	GtkWidget *container;
	GladeXML *xml;

	if (!panel)
		return -EINVAL;

	xml = glade_xml_new("medcom-client-gtk.glade", "tuner_panel", NULL);
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
