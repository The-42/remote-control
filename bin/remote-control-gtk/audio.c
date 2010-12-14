#include <string.h>
#include "medcom-client-gtk.h"

#define USER_DATA_KEY        "s7zasdv"

#define LEGACY_AUDIO_SUPPORT 0

struct panel_data {
	struct medcom_client *client;
	/* the currently select mixer control */
	enum medcom_mixer_control mixer_value;
	enum medcom_mixer_input_source input_source;
	/* the mixer controls */
	GtkComboBox *mixer_combo;
	GtkHScale *mixer_volume;
	GtkCheckButton *mixer_muted;
	/* the input controls */
	GtkComboBox *input_combo;
	GtkLabel *input_current;
};

struct control_mapping {
	int control_id;
	const gchar* text;
};

static const struct control_mapping mixer_map[] = {
	{ MEDCOM_MIXER_CONTROL_PLAYBACK_MASTER,  "Playback - Master" },
	{ MEDCOM_MIXER_CONTROL_PLAYBACK_PCM,     "Playback - PCM" },
	{ MEDCOM_MIXER_CONTROL_PLAYBACK_HEADSET, "Playback - Headset" },
	{ MEDCOM_MIXER_CONTROL_PLAYBACK_SPEAKER, "Playback - Speaker" },
	{ MEDCOM_MIXER_CONTROL_PLAYBACK_HANDSET, "Playback - Handset" },

	{ MEDCOM_MIXER_CONTROL_CAPTURE_MASTER,   "Capture - Master" },
	{ MEDCOM_MIXER_CONTROL_CAPTURE_HEADSET,  "Capture - Headset" },
	{ MEDCOM_MIXER_CONTROL_CAPTURE_HANDSET,  "Capture - Handset" },
	{ MEDCOM_MIXER_CONTROL_CAPTURE_LINE,     "Capture - Line"},
	{ 0, NULL }
};

static const struct control_mapping input_map[] = {
//	{ MEDCOM_MIXER_INPUT_SOURCE_UNKNOWN, "Unknown" },
	{ MEDCOM_MIXER_INPUT_SOURCE_HANDSET, "Headset" },
	{ MEDCOM_MIXER_INPUT_SOURCE_HEADSET, "Handset" },
	{ MEDCOM_MIXER_INPUT_SOURCE_LINE,    "Line" },
	{ 0, NULL }
};


static enum medcom_mixer_control mixer_name_to_id(const gchar* text)
{
	int i;

	for (i=0; i<G_N_ELEMENTS(mixer_map); i++)
		if (g_strcmp0(mixer_map[i].text, text) == 0)
			return mixer_map[i].control_id;

	return MEDCOM_MIXER_CONTROL_MAX;
}

static enum medcom_mixer_input_source input_name_to_id(const gchar* text)
{
	int i;

	for (i=0; i<G_N_ELEMENTS(input_map); i++)
		if (g_strcmp0(input_map[i].text, text) == 0)
			return input_map[i].control_id;

	return MEDCOM_MIXER_INPUT_SOURCE_UNKNOWN;
}
/*
static void audio_panel_dump_priv(struct panel_data *priv)
{
	fprintf(stdout, "  panel_data:       %p\n", priv);
	fprintf(stdout, "   mixer_value:     %d\n", priv->mixer_value);
	fprintf(stdout, "    mixer_client:   %p\n", priv->mixer_combo);
	fprintf(stdout, "    mixer_volume:   %p\n", priv->mixer_volume);
	fprintf(stdout, "    mixer_muted:    %p\n", priv->mixer_muted);
	fprintf(stdout, "   input_source:    %d\n", priv->input_source);
}
*/
static int audio_panel_update_mixer_controls(struct panel_data *pnl)
{
	uint8_t volume;
	bool muted;
	int ret;

	if (!pnl)
		return -EINVAL;

	ret = medcom_mixer_get_volume(pnl->client, pnl->mixer_value, &volume);
	if (ret < 0) {
		printf(" medcom_mixer_get_volume(%p, %d, ...) failed with %d\n",
			pnl->client, pnl->mixer_value, ret);
		return ret;
	}

	gtk_range_set_value(GTK_RANGE(pnl->mixer_volume), (gdouble)volume);

	ret = medcom_mixer_get_mute(pnl->client, pnl->mixer_value, &muted);
	if (ret < 0) {
		printf(" medcom_mixer_get_mute(%p, %d, ...) failed with %d\n",
			pnl->client, pnl->mixer_value, ret);
		return ret;
	}
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pnl->mixer_muted), muted);

	return 0;
}

static int audio_panel_update_input_controls(struct panel_data *pnl)
{
	enum medcom_mixer_input_source source = MEDCOM_MIXER_INPUT_SOURCE_UNKNOWN;
	int32_t ret;
	int i;

	if (!pnl)
		return -EINVAL;

	ret = medcom_mixer_set_input_source(pnl->client, pnl->input_source);
	if (ret < 0) {
		printf(" medcom_mixer_set_input_source(%p, %d) failed with %d\n",
			pnl->client, pnl->input_source, ret);
		return ret;
	}

	ret = medcom_mixer_get_input_source(pnl->client, &source);
	if (ret < 0) {
		printf(" medcom_mixer_get_input_source(%p, ...) failed with %d\n",
			pnl->client, ret);
		return ret;
	}

	for (i=0; input_map[i].text != NULL; i++)
		if (source == input_map[i].control_id) {
			gtk_label_set_text(GTK_LABEL(pnl->input_current), input_map[i].text);
			return 1;
		}

	gtk_label_set_text(GTK_LABEL(pnl->input_current), "Unknown");
	return 0;
}

void on_mixer_channel_changed(GtkComboBox *combo, gpointer data)
{
	struct panel_data *pnl;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *string = NULL;

	printf("> %s(widget=%p, data=%p)\n", __func__, (void *)combo, data);

	pnl = (struct panel_data*)g_object_get_data(G_OBJECT(combo), USER_DATA_KEY);
	if (!pnl) {
		printf("ERR:%s: no panel data\n", __func__);
		goto out;
	}

	/* Obtain currently selected item from combo box. If nothing is
	 * selected, do nothing. */
	if (gtk_combo_box_get_active_iter(combo, &iter)) {
		/* Obtain data model from combo box. */
		model = gtk_combo_box_get_model(combo);
		/* Obtain string from model. */
		gtk_tree_model_get( model, &iter, 0, &string, -1 );
	}

	/* Free string (if not NULL). */
	if (string) {
		pnl->mixer_value = mixer_name_to_id(string);
		audio_panel_update_mixer_controls(pnl);

		printf("   selected:...%s\n", string);
		printf("   channel:....%d\n", pnl->mixer_value);

		g_free(string);
	}
out:
	printf("< %s()\n", __func__);
}

void on_mixer_channel_volume_changed(GtkWidget *widget, gpointer data)
{
	struct panel_data *pnl;
	gdouble val;
	int32_t ret;

	printf("> %s(widget=%p, data=%p)\n", __func__, (void *)widget, data);

	pnl = (struct panel_data*)g_object_get_data(G_OBJECT(widget), USER_DATA_KEY);
	if (!pnl) {
		printf("ERR:%s: no panel data\n", __func__);
		goto out;
	}

	val = gtk_range_get_value(GTK_RANGE(widget));

	ret = medcom_mixer_set_volume(pnl->client, pnl->mixer_value, (uint8_t)val);
	if (ret < 0) {
		printf("ERR:%s: medcom_mixer_set_volume failed %d\n", __func__, ret);
	}

out:
	printf("< %s()\n", __func__);
}

void on_mixer_channel_mute_toggled(GtkWidget *widget, gpointer data)
{
	struct panel_data *pnl;
	gboolean mute;
	int32_t ret;

	printf("> %s(widget=%p, data=%p)\n", __func__, (void *)widget, data);

	pnl = (struct panel_data*)g_object_get_data(G_OBJECT(widget), USER_DATA_KEY);
	if (!pnl) {
		printf("ERR:%s: no panel data\n", __func__);
		goto out;
	}

	mute = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

	ret = medcom_mixer_set_mute(pnl->client, pnl->mixer_value, mute);
	if (ret < 0) {
		printf("ERR:%s: medcom_mixer_set_mute failed %d\n", __func__, ret);
	}

out:
	printf("< %s()\n", __func__);
}

void on_input_source_changed(GtkComboBox *combo, gpointer data)
{
	struct panel_data *pnl;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *string = NULL;

	printf("> %s(widget=%p, data=%p)\n", __func__, (void *)combo, data);

	pnl = (struct panel_data*)g_object_get_data(G_OBJECT(combo), USER_DATA_KEY);
	if (!pnl) {
		printf("ERR:%s: no panel data\n", __func__);
		goto out;
	}

	/* Obtain currently selected item from combo box. If nothing is
	 * selected, do nothing. */
	if (gtk_combo_box_get_active_iter(combo, &iter)) {
		/* Obtain data model from combo box. */
		model = gtk_combo_box_get_model(combo);
		/* Obtain string from model. */
		gtk_tree_model_get( model, &iter, 0, &string, -1 );
	}

	/* Free string (if not NULL). */
	if (string) {
		pnl->input_source = input_name_to_id(string);
		audio_panel_update_input_controls(pnl);

		printf("   selected:........%s\n", string);
		printf("   input_source:....%d\n", pnl->input_source);

		g_free(string);
	}
out:
	printf("< %s()\n", __func__);
}

void on_legacy_volume_value_changed(GtkWidget *widget, gpointer data)
{
#if LEGACY_AUDIO_SUPPORT
	gdouble val;
	int32_t ret;

	printf("> %s(widget=%p, data=%p)\n", __func__, (void *)widget, data);

	val = gtk_range_get_value(GTK_RANGE(widget));

	ret = medcom_audio_set_volume(g_client, (uint8_t)val);
	if (ret < 0) {
		printf("ERR:%s: medcom_audio_set_volume failed %d\n", __func__, ret);
	}
#endif
}

void on_legacy_speakers_enable_toggled(GtkWidget *widget, gpointer data)
{
#if LEGACY_AUDIO_SUPPORT
	gboolean enabled;
	int ret;

	enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

	ret = medcom_audio_enable_speakers(g_client, enabled);
	if (ret < 0) {
		fprintf(stderr, "failed to %s speakers: %s\n",
				enabled ? "disable" : "enable",
				strerror(-ret));
	} else {
		printf("speakers: %s\n", enabled ? "enabled" : "disabled");
	}
#endif
}

static int audio_panel_fill_combo_box(GtkComboBox *combo, const struct control_mapping table[])
{
	GtkTreeModel *model;

	model = gtk_combo_box_get_model(combo);
	if (!model) {
		GtkCellRenderer *renderer;
		GtkListStore *list;
		GtkTreeIter iter;
		int i;

		list = gtk_list_store_new(1, G_TYPE_STRING);

		for (i=0; table[i].text != NULL; i++) {
			fprintf(stdout, "table[%d].control_id:. %d\n", i, table[i].control_id);
			fprintf(stdout, "table[%d].text:....... %s\n", i, table[i].text);
			gtk_list_store_append(list, &iter);
			gtk_list_store_set(list, &iter, 0, table[i].text, -1);
		}

		gtk_combo_box_set_model(GTK_COMBO_BOX(combo), GTK_TREE_MODEL(list));

		renderer = gtk_cell_renderer_text_new();
		gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), renderer, FALSE);
		gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer, "text", 0, NULL);

		gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);

		g_object_unref(list);
	}

	return 0;
}

static int audio_panel_fill_mixer_combo_box(GtkComboBox *combo)
{
	return audio_panel_fill_combo_box(combo, mixer_map);
}

static int audio_panel_fill_input_combo_box(GtkComboBox *combo)
{
	return audio_panel_fill_combo_box(combo, input_map);
}

static int audio_panel_create_mixer(GladeXML *xml, struct panel_data *priv)
{
	priv->mixer_value = MEDCOM_MIXER_CONTROL_PLAYBACK_MASTER;

	priv->mixer_combo = GTK_COMBO_BOX(glade_xml_get_widget(xml, "channel_select"));
	if (priv->mixer_combo)
		g_object_set_data(G_OBJECT(priv->mixer_combo), USER_DATA_KEY, priv);

	priv->mixer_volume = GTK_HSCALE(glade_xml_get_widget(xml, "channel_volume"));
	if (priv->mixer_volume)
		g_object_set_data(G_OBJECT(priv->mixer_volume), USER_DATA_KEY, priv);

	priv->mixer_muted  = GTK_CHECK_BUTTON(glade_xml_get_widget(xml, "channel_mute"));
	if (priv->mixer_muted)
		g_object_set_data(G_OBJECT(priv->mixer_muted), USER_DATA_KEY, priv);

	/* fill the combo box only, when we have processed all widgets */
	if (priv->mixer_combo)
		audio_panel_fill_mixer_combo_box(priv->mixer_combo);

	return 0;
}

static int audio_panel_create_input(GladeXML *xml, struct panel_data *priv)
{
	priv->input_source = MEDCOM_MIXER_INPUT_SOURCE_UNKNOWN;

	priv->input_combo = GTK_COMBO_BOX(glade_xml_get_widget(xml, "frame_input_combobox_source"));
	if (priv->input_combo)
		g_object_set_data(G_OBJECT(priv->input_combo), USER_DATA_KEY, priv);

	priv->input_current = GTK_LABEL(glade_xml_get_widget(xml, "frame_input_label_current"));
	if (priv->input_current)
		g_object_set_data(G_OBJECT(priv->input_current), USER_DATA_KEY, priv);


	/* fill the combo box only, when we have processed all widgets */
	if (priv->input_combo)
		audio_panel_fill_input_combo_box(priv->input_combo);

	return 0;
}

static int audio_panel_create_legacy(GladeXML *xml, struct panel_data *priv)
{
	GtkWidget *legacy;

#if !LEGACY_AUDIO_SUPPORT
	legacy = glade_xml_get_widget(xml, "frame_legacy");
	if (legacy)
		gtk_widget_set_sensitive(legacy, FALSE);
#else
	legacy = glade_xml_get_widget(xml, "enable");
	if (legacy) {
		bool enabled = false;
		int ret;

		ret = medcom_audio_speakers_enabled(g_client, &enabled);
		if (ret < 0)
			fprintf(stderr, "failed to get audio speakers status\n");

		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(legacy), enabled);
	}
#endif
	return 0;
}

static int audio_panel_create(struct panel *panel, GtkWidget **widget)
{
	struct panel_data *priv;
	GtkWidget *container;
	GladeXML *xml;

	if (!panel)
		return -EINVAL;

	xml = glade_xml_new("medcom-client-gtk.glade", "audio_panel", NULL);
	glade_xml_signal_autoconnect(xml);

	container = glade_xml_get_widget(xml, "audio_panel");
	if (!container) {
		fprintf(stderr, "failed to get audio panel\n");
		return -ENOENT;
	}

	priv = malloc(sizeof(struct panel_data));
	if (!priv)
		return -ENOMEM;
	memset(priv, 0, sizeof(*priv));

	panel->priv = priv;

	priv->client = g_client;

	audio_panel_create_mixer(xml, priv);
	audio_panel_create_input(xml, priv);
	audio_panel_create_legacy(xml, priv);

	*widget = container;

	return 0;
}

static int audio_panel_destroy(struct panel *panel)
{
	if (!panel)
		return -EINVAL;

	if (panel->priv) {
		free(panel->priv);
		panel->priv = NULL;
	}

	return 0;
}

struct panel audio_panel = {
	.name = "Audio",
	.create = audio_panel_create,
	.destroy = audio_panel_destroy,
};
