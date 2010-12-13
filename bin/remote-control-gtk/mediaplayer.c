#include <string.h>

#include "medcom-client-gtk.h"

struct mediaplayer_panel {
	unsigned int x, y, width, height;
	gchar *url;
};

void on_mediaplayer_start_clicked(GtkWidget *widget, gpointer data)
{
	struct mediaplayer_panel *mpp;
	int ret;

	mpp = g_object_get_data(G_OBJECT(widget), "user");

	ret = medcom_media_player_set_output_window(g_client, mpp->x, mpp->y,
			mpp->width, mpp->height);
	if (ret < 0) {
		fprintf(stderr, "failed to set video output window: %d\n",
				ret);
		return;
	}

	ret = medcom_media_player_set_stream(g_client, mpp->url);
	if (ret < 0) {
		fprintf(stderr, "failed to set stream: %d\n", ret);
		return;
	}

	ret = medcom_media_player_start(g_client);
	if (ret < 0) {
		fprintf(stderr, "failed to start stream: %d\n", ret);
		return;
	}
}

void on_mediaplayer_stop_clicked(GtkWidget *widget, gpointer data)
{
	int ret;

	ret = medcom_media_player_stop(g_client);
	if (ret < 0) {
		fprintf(stderr, "failed to stop stream: %d\n", ret);
		return;
	}
}

static int get_entry_value(GtkEntry *entry, unsigned int *value)
{
	const gchar *text;
	gchar *end = NULL;
	unsigned int val;

	if (!entry || !value)
		return -EINVAL;

	text = gtk_entry_get_text(entry);
	val = strtoul(text, &end, 0);
	if (end == text)
		return -EINVAL;

	*value = val;
	return 0;
}

void on_url_changed(GtkWidget *widget, gpointer data)
{
	struct mediaplayer_panel *mpp;
	const gchar *url;

	mpp = g_object_get_data(G_OBJECT(widget), "user");
	url = gtk_entry_get_text(GTK_ENTRY(widget));

	if (mpp->url)
		free(mpp->url);

	mpp->url = g_strdup(url);
}

void on_position_x_changed(GtkWidget *widget, gpointer data)
{
	struct mediaplayer_panel *mpp = g_object_get_data(G_OBJECT(widget), "user");
	unsigned int value = 0;
	int ret = 0;

	ret = get_entry_value(GTK_ENTRY(widget), &value);
	if (ret < 0)
		return;

	if (mpp)
		mpp->x = value;
}

void on_position_y_changed(GtkWidget *widget, gpointer data)
{
	struct mediaplayer_panel *mpp = g_object_get_data(G_OBJECT(widget), "user");
	unsigned int value = 0;
	int ret = 0;

	ret = get_entry_value(GTK_ENTRY(widget), &value);
	if (ret < 0)
		return;

	if (mpp)
		mpp->y = value;
}

void on_size_x_changed(GtkWidget *widget, gpointer data)
{
	struct mediaplayer_panel *mpp = g_object_get_data(G_OBJECT(widget), "user");
	unsigned int value = 0;
	int ret = 0;

	ret = get_entry_value(GTK_ENTRY(widget), &value);
	if (ret < 0)
		return;

	if (mpp)
		mpp->width = value;
}

void on_size_y_changed(GtkWidget *widget, gpointer data)
{
	struct mediaplayer_panel *mpp = g_object_get_data(G_OBJECT(widget), "user");
	unsigned int value = 0;
	int ret = 0;

	ret = get_entry_value(GTK_ENTRY(widget), &value);
	if (ret < 0)
		return;

	if (mpp)
		mpp->height = value;
}

static int mediaplayer_panel_create(struct panel *panel, GtkWidget **widget)
{
	struct mediaplayer_panel *mpp = NULL;
	GtkWidget *container;
	GtkWidget *entry;
	char buffer[16];
	GladeXML *xml;

	if (!panel)
		return -EINVAL;

	if (!panel->priv) {
		mpp = malloc(sizeof(*mpp));
		if (!mpp)
			return -ENOMEM;

		memset(mpp, 0, sizeof(*mpp));
		mpp->width = 720;
		mpp->height = 576;

		panel->priv = mpp;
	} else {
		mpp = panel->priv;
	}

	xml = glade_xml_new("medcom-client-gtk.glade", "mediaplayer_panel", NULL);
	glade_xml_signal_autoconnect(xml);

	entry = glade_xml_get_widget(xml, "url");
	if (entry) {
		g_object_set_data(G_OBJECT(entry), "user", mpp);

		if (mpp->url != NULL)
			gtk_entry_set_text(GTK_ENTRY(entry), mpp->url);
	}

	entry = glade_xml_get_widget(xml, "entry_position_x");
	if (entry) {
		snprintf(buffer, sizeof(buffer), "%u", mpp->x);
		gtk_entry_set_text(GTK_ENTRY(entry), buffer);
		g_object_set_data(G_OBJECT(entry), "user", mpp);
	}

	entry = glade_xml_get_widget(xml, "entry_position_y");
	if (entry) {
		snprintf(buffer, sizeof(buffer), "%u", mpp->y);
		gtk_entry_set_text(GTK_ENTRY(entry), buffer);
		g_object_set_data(G_OBJECT(entry), "user", mpp);
	}

	entry = glade_xml_get_widget(xml, "entry_size_x");
	if (entry) {
		snprintf(buffer, sizeof(buffer), "%u", mpp->width);
		gtk_entry_set_text(GTK_ENTRY(entry), buffer);
		g_object_set_data(G_OBJECT(entry), "user", mpp);
	}

	entry = glade_xml_get_widget(xml, "entry_size_y");
	if (entry) {
		snprintf(buffer, sizeof(buffer), "%u", mpp->height);
		gtk_entry_set_text(GTK_ENTRY(entry), buffer);
		g_object_set_data(G_OBJECT(entry), "user", mpp);
	}

	container = glade_xml_get_widget(xml, "mediaplayer_panel");
	if (!container) {
		fprintf(stderr, "failed to get media player panel\n");
		return -ENOENT;
	}

	*widget = container;
	return 0;
}

static int mediaplayer_panel_destroy(struct panel *panel)
{
	if (!panel)
		return -EINVAL;

	if (panel->priv) {
		free(panel->priv);
		panel->priv = NULL;
	}

	return 0;
}

struct panel mediaplayer_panel = {
	.name = "Media Player",
	.create = mediaplayer_panel_create,
	.destroy = mediaplayer_panel_destroy,
};
