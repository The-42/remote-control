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

#include <string.h>

#include "remote-control-gtk.h"

extern struct panel audio_panel;
extern struct panel backlight_panel;
extern struct panel card_panel;
extern struct panel mediaplayer_panel;
extern struct panel miscellaneous_panel;
extern struct panel voip_panel;
extern struct panel webkit_panel;

static struct panel *panels[] = {
	&audio_panel,
	&backlight_panel,
	&card_panel,
	&mediaplayer_panel,
	&miscellaneous_panel,
	&voip_panel,
	&webkit_panel,
};

/* TODO: get rid of this global variable */
struct remote_client *g_client = NULL;

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

void on_destroy(void)
{
	gtk_main_quit();
}

void on_menu_help_about_activate(GtkWidget *widget, gpointer ptr)
{
	GtkWidget *dialog;
	GladeXML *xml;

	xml = glade_xml_new(PKG_DATA_DIR "/remote-control-gtk.glade", "about_dialog", NULL);
	glade_xml_signal_autoconnect(xml);

	dialog = glade_xml_get_widget(xml, "about_dialog");
	gtk_dialog_run(GTK_DIALOG(dialog));
}

/**
 * This is called when ever the item on the left menu list is changed.
 * So we can view the menu that belongs to the menu entry.
 */
void on_panel_select_changed(GtkWidget *tree, gpointer ptr)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model = NULL;
	struct panel *panel = NULL;
	GtkWidget *parent = NULL;
	GtkWidget *widget = NULL;
	GtkWidget *viewport;
	GtkTreeIter iter;
	gboolean err;
	int ret;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));

	err = gtk_tree_selection_get_selected(selection, &model, &iter);
	if (!err)
		return;

	gtk_tree_model_get(model, &iter, 2, &parent, -1);

	if (!parent)
		return;

	viewport = gtk_bin_get_child(GTK_BIN(parent));
	if (viewport) {
		GtkWidget *old = gtk_bin_get_child(GTK_BIN(viewport));
		panel = g_object_get_data(G_OBJECT(old), "user");

		if (panel && panel->close)
			panel->close(panel);

		gtk_container_remove(GTK_CONTAINER(parent), viewport);
	}

	gtk_tree_model_get(model, &iter, 1, &panel, -1);

	if (!panel || !panel->create)
		return;

	ret = panel->create(panel, &widget);
	if (ret < 0) {
		fprintf(stderr, "failed to create widget: %s %d\n", panel->name, ret);
		return;
	}

#if GTK_CHECK_VERSION(3, 8, 0)
	gtk_container_add(GTK_CONTAINER(parent), widget);
#else
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(parent), widget);
#endif
	g_object_set_data(G_OBJECT(widget), "user", panel);
}

static void usage(FILE *fp, const char *program)
{
	fprintf(fp, "usage: %s url\n", program);
}

int main(int argc, char *argv[])
{
	struct remote_client *client = NULL;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkListStore *store;
	GtkWidget *window;
	GtkWidget *active;
	GtkTreeIter iter;
	GtkWidget *list;
	unsigned int i;
	GladeXML *xml;
	int ret = 0;

	gtk_init(&argc, &argv);

	if (argc < 2) {
		usage(stderr, argv[0]);
		return 1;
	}

	ret = remote_init(&client, argv[1], NULL);
	if (ret < 0) {
		fprintf(stderr, "failed to initialize: %s\n",
				strerror(-ret));
		return 1;
	}

	g_client = client;

	xml = glade_xml_new(GLADE_FILE, "main_window", NULL);
	glade_xml_signal_autoconnect(xml);

	window = glade_xml_get_widget(xml, "main_window");
	active = glade_xml_get_widget(xml, "panel_active");
	list = glade_xml_get_widget(xml, "panel_select");

	store = gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_POINTER);

	/* setup list box */
	for (i = 0; i < ARRAY_SIZE(panels); i++) {
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
				0, panels[i]->name,
				1, panels[i],
				2, active, -1);
	}

	gtk_tree_view_set_model(GTK_TREE_VIEW(list), GTK_TREE_MODEL(store));
	g_object_unref(store);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(NULL, renderer,
			"text", 0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);

	gtk_window_set_default_size(GTK_WINDOW(window), 640, 480);
	gtk_widget_show(window);

	gtk_main();

	remote_exit(client);
	return 0;
}
