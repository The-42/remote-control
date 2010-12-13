#include "medcom-client-gtk.h"
#include "voip.h"

#define USER_DATA_KEY "user"

struct panel_data {
	/* connection */
	GtkEntry *server_address;
	GtkEntry *server_port;
	GtkEntry *server_username;
	GtkEntry *server_password;
	GtkLabel *connection_status;
	/* call */
	GtkEntry *call_number;
};

static void voip_panel_dump(struct panel_data *pnl)
{
	fprintf(stdout, " Panel:               %p\n", pnl);
	fprintf(stdout, "   server_address:    %p\n", pnl->server_address);
	fprintf(stdout, "   server_port:       %p\n", pnl->server_port);
	fprintf(stdout, "   server_username:   %p\n", pnl->server_username);
	fprintf(stdout, "   server_password:   %p\n", pnl->server_password);
	fprintf(stdout, "   connection_status: %p\n", pnl->connection_status);
	fprintf(stdout, "   call_number:       %p\n", pnl->call_number);
}

static void voip_panel_destroy_userdata(gpointer data)
{
	fprintf(stdout, "%s: data: %p\n", __func__, data);
}

int voip_panel_append(GtkWidget *widget, const gchar* to_append)
{
	GtkEntry *entry;

	entry = GTK_ENTRY(widget);
	if (!entry) {
		fprintf(stderr, "%s not an entry\n", __func__);
		return -EINVAL;
	}

	gtk_entry_append_text(entry, to_append);
	return 0;
}

void on_voip_number_button_undo_clicked(GtkWidget *widget, gpointer data)
{
	GtkEntryBuffer* buffer;
	GtkEntry *entry;
	guint length;
	gchar* text;

	entry = GTK_ENTRY(widget);
	if (!entry) {
		fprintf(stderr, "%s not an entry\n", __func__);
		return;
	}

	buffer = gtk_entry_get_buffer(entry);
	if (!buffer) {
		fprintf(stderr, "%s no buffer in entry\n", __func__);
		return;
	}

	length = gtk_entry_buffer_get_length(buffer);
	if (length < 1) {
		fprintf(stdout, "entry is empty\n");
		return;
	}

	text = g_strdup(gtk_entry_buffer_get_text(buffer));
	text[length-1] = '\0';

	gtk_entry_buffer_set_text(buffer, text, length-1);
	gtk_entry_set_buffer(entry, buffer);

	g_free(text);
}

void on_voip_server_button_login_clicked(GtkWidget *widget, gpointer data)
{
	struct medcom_voip_account login;
	struct panel_data *pnl = NULL;
	int32_t ret;

	fprintf(stdout, "> %s(widget=%p, data=%p)\n", __func__, widget, data);

	if (!widget) {
		fprintf(stderr, " widget not valid\n");
		goto out;
	}

	pnl = (struct panel_data*)g_object_get_data(G_OBJECT(widget), USER_DATA_KEY);
	if (!pnl) {
		fprintf(stderr, " panel not valid\n");
		goto out;
	}
	voip_panel_dump(pnl);

	if (!pnl->server_address || !pnl->server_port ||
	    !pnl->server_username || !pnl->server_password)
	{
		fprintf(stderr, " panel not complete %p, %p, %p, %p\n",
			pnl->server_address, pnl->server_port,
			pnl->server_username, pnl->server_password);
		goto out;
	}

	memset(&login, 0, sizeof(login));

	login.server   = g_strdup(gtk_entry_get_text(pnl->server_address));
	if (sscanf(gtk_entry_get_text(pnl->server_port), "%hd", &login.port) != 1)
		login.port = 5060;
	login.username = g_strdup(gtk_entry_get_text(pnl->server_username));
	login.password = g_strdup(gtk_entry_get_text(pnl->server_password));

	ret = medcom_voip_login(g_client, &login);
	if (ret < 0) {
		fprintf(stderr, "login failed %d\n", ret);
		goto out;
	}

out:
//	g_free(login.server);
//	g_free(login.username);
//	g_free(login.password);
	fprintf(stdout, "< %s()\n", __func__);
}

void on_voip_server_button_logout_clicked(GtkWidget *widget, gpointer data)
{
	fprintf(stdout, "%s\n", __func__);
}

void on_voip_button_hook_clicked(GtkWidget *widget, gpointer data)
{
	fprintf(stdout, "%s\n", __func__);
}

void on_voip_button_unhook_clicked(GtkWidget *widget, gpointer data)
{
	fprintf(stdout, "%s\n", __func__);
}

static int voip_panel_create(struct panel *panel, GtkWidget **widget)
{
	struct panel_data *priv;;
	GtkWidget *container;
	GladeXML *xml;

	if (!panel)
		return -EINVAL;

	xml = glade_xml_new("medcom-client-gtk.glade", "voip_panel", NULL);
	if (!xml) {
		fprintf(stderr, "failed to open glade file\n");
		return -ENOENT;
	}
	glade_xml_signal_autoconnect(xml);

	container = glade_xml_get_widget(xml, "voip_panel");
	if (!container) {
		fprintf(stderr, "failed to get voip panel\n");
		return -ENOENT;
	}

	priv = malloc(sizeof(struct panel_data));
	if (priv) {
		memset(priv, 0, sizeof(*priv));

		priv->server_address    = GTK_ENTRY(glade_xml_get_widget(xml, "voip_connection_server_addr_entry"));
		priv->server_port       = GTK_ENTRY(glade_xml_get_widget(xml, "voip_connection_server_port_entry"));
		priv->server_username   = GTK_ENTRY(glade_xml_get_widget(xml, "voip_connection_username_entry"));
		priv->server_password   = GTK_ENTRY(glade_xml_get_widget(xml, "voip_connection_password_entry"));
		priv->connection_status = GTK_LABEL(glade_xml_get_widget(xml, "voip_connection_status_label"));

		priv->call_number       = GTK_ENTRY(glade_xml_get_widget(xml, "voip_number_entry"));

		panel->priv = priv;

		fprintf(stdout, "voip_panel=%p\n", container);
		voip_panel_dump(priv);
		g_object_set_data_full(G_OBJECT(container), USER_DATA_KEY, priv, voip_panel_destroy_userdata);
	}

	*widget = container;
	return 0;
}

static int voip_panel_destroy(struct panel *panel)
{
	if (!panel)
		return -EINVAL;

	if (panel->priv) {
		free(panel->priv);
		panel->priv = NULL;
	}

	return 0;
}

struct panel voip_panel = {
	.name = "VoIP",
	.create = voip_panel_create,
	.destroy = voip_panel_destroy,
};
