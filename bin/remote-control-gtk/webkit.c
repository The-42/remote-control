/*
 * Copyright (C) 2010-2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <webkit/webkit.h>

#include "medcom-client-gtk.h"

static int webkit_panel_create(struct panel *panel, GtkWidget **widget)
{
	WebKitWebView *webkit;

	if (!g_thread_supported())
		g_thread_init(NULL);

	webkit = WEBKIT_WEB_VIEW(webkit_web_view_new());
	if (!webkit) {
		fprintf(stderr, "failed to embed WebKit browser\n");
		return -EIO;
	}

	//webkit_web_view_load_uri(webkit, "http://www.google.com/ncr");
	webkit_web_view_load_uri(webkit, "http://rdp-0001/hotel_web/index.html?ID=001");
	gtk_widget_show(GTK_WIDGET(webkit));
	*widget = GTK_WIDGET(webkit);

	return 0;
}

struct panel webkit_panel = {
	.name = "WebKit",
	.create = webkit_panel_create,
	.destroy = NULL,
};

