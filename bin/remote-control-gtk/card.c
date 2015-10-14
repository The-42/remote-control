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

#include <stdlib.h>
#include <string.h>

#include "remote-control-gtk.h"

struct card_panel {
	GtkWidget *widget;
	gboolean enabled;
};

struct card_context {
	struct card_panel *cp;
	uint32_t type;
};

struct card_type_mapping {
	enum remote_card_type type;
	const char *name;
};

#define DEFINE_CARD_TYPE(_type, _name) \
	{ .type = _type, .name = _name }

#define ARRAY_SIZE(a) (sizeof(a) / sizeof ((a)[0]))

static struct card_type_mapping card_type_name[] = {
	DEFINE_CARD_TYPE(REMOTE_CARD_TYPE_UNKNOWN, "unknown"),
	DEFINE_CARD_TYPE(REMOTE_CARD_TYPE_I2C, "I2C"),
};

gpointer card_thread(gpointer data)
{
	struct card_context *context = data;
	struct card_panel *cp = context->cp;

	if (context->type == 0)
		cp->enabled = FALSE;
	else
		cp->enabled = TRUE;

	if (cp->widget) {
		gtk_widget_set_sensitive(cp->widget, cp->enabled);
		gtk_widget_queue_draw(cp->widget);
	}

	free(context);
	return NULL;
}

void on_card_event(uint32_t type, void *data)
{
	struct card_context *context;
#if !GLIB_CHECK_VERSION(2, 31, 0)
	GError *error;
#endif

	context = malloc(sizeof(*context));
	if (!context)
		return;

	context->type = type;
	context->cp = data;

#if GLIB_CHECK_VERSION(2, 31, 0)
	g_thread_new("card-event", card_thread, context);
#else
	g_thread_create(card_thread, context, TRUE, &error);
#endif
}

void on_refresh_clicked(GtkWidget *widget, gpointer data)
{
	enum remote_card_type type = REMOTE_CARD_TYPE_UNKNOWN;
	gchar buf[32];
	int err;
	int i;

	err = remote_card_get_type(g_client, &type);
	if (err < 0) {
		fprintf(stderr, "failed to get card type: %d\n", err);
		return;
	}

	for (i = 0; i < ARRAY_SIZE(card_type_name); i++) {
		if (card_type_name[i].type == type) {
			gtk_label_set_text(GTK_LABEL(widget),
					card_type_name[i].name);
			break;
		}
	}

	if (i >= ARRAY_SIZE(card_type_name)) {
		g_snprintf(buf, sizeof(buf), "unknown type: %d", type);
		gtk_label_set_text(GTK_LABEL(widget), buf);
	}
}

void on_read_clicked(GtkWidget *widget, gpointer user_data)
{
	GtkTextView *view = GTK_TEXT_VIEW(widget);
	const unsigned int cols = 16;
	PangoFontDescription *font;
	GtkTextBuffer *buffer;
	unsigned int i, j;
	char text[512];
	char data[64];
	int32_t ret;
	int len = 0;
	int num;

	memset(text, 0, sizeof(text));
	memset(data, 0, sizeof(data));

	ret = remote_card_read(g_client, 0, data, sizeof(data));
	if (ret < 0) {
		fprintf(stderr, "failed to read card: %d\n", ret);
		return;
	}

	font = pango_font_description_from_string("Monospace");
	gtk_widget_modify_font(GTK_WIDGET(view), font);

	buffer = gtk_text_view_get_buffer(view);

	for (i = 0; i < ret; i += cols) {
		if (i > 0) {
			num = snprintf(text + len, sizeof(text) - len, "\n");
			if (num < 0) {
				fprintf(stderr, "no space left: %d\n", len);
				break;
			}

			len += num;
		}

		for (j = 0; (j < cols) && ((i + j) < ret); j++) {
			num = snprintf(text + len, sizeof(text) - len, " %02x",
					(unsigned char)data[i + j]);
			if (num <= 0) {
				fprintf(stderr, "no space left: %d\n", len);
				break;
			}

			len += num;
		}
	}

	gtk_text_buffer_set_text(buffer, text, -1);
}

void on_write_clicked(GtkWidget *widget, gpointer data)
{
	fprintf(stderr, "NOT IMPLEMENTED: %s\n", __func__);
}

static int card_panel_create(struct panel *panel, GtkWidget **widget)
{
	struct card_panel *cp;
	GtkWidget *container;
	GladeXML *xml;

	if (!panel)
		return -EINVAL;

	if (!panel->priv) {
#if !GLIB_CHECK_VERSION(2, 31, 0)
		if (!g_thread_supported())
			g_thread_init(NULL);
#endif

		cp = malloc(sizeof(*cp));
		if (!cp)
			return -ENOMEM;

		cp->enabled = TRUE;
		cp->widget = NULL;

		remote_register_event_handler(g_client, REMOTE_EVENT_CARD,
				on_card_event, cp);
		panel->priv = cp;
	} else {
		cp = panel->priv;
	}

	xml = glade_xml_new(GLADE_FILE, "card_panel", NULL);
	glade_xml_signal_autoconnect(xml);

	container = glade_xml_get_widget(xml, "card_panel");
	if (!container)
		return -ENOENT;

	printf("%s panel\n", cp->enabled ? "enabling" : "disabling");
	gtk_widget_set_sensitive(container, cp->enabled);
	cp->widget = container;
	*widget = container;

	return 0;
}

static int card_panel_close(struct panel *panel)
{
	struct card_panel *cp;

	if (!panel)
		return -EINVAL;

	cp = panel->priv;
	cp->widget = NULL;

	return 0;
}

static int card_panel_destroy(struct panel *panel)
{
	printf("> %s()\n", __func__);

	if (!panel)
		return -EINVAL;

	remote_unregister_event_handler(g_client, REMOTE_EVENT_CARD,
			on_card_event);

	if (panel->priv) {
		free(panel->priv);
		panel->priv = NULL;
	}

	printf("< %s()\n", __func__);
	return 0;
}

struct panel card_panel = {
	.name = "SmartCard",
	.create = card_panel_create,
	.close = card_panel_close,
	.destroy = card_panel_destroy,
};
