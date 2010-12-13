#include "medcom-client-gtk.h"

static int modem_panel_create(struct panel *panel, GtkWidget **widget)
{
	return -ENOSYS;
}

struct panel modem_panel = {
	.name = "Modem",
	.create = modem_panel_create,
	.destroy = NULL,
};
