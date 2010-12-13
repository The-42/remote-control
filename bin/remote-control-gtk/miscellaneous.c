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
