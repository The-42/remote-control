/*
 * Copyright (C) 2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <libsoup/soup-session.h>
#include <libsoup/soup-uri.h>

void soup_session_set_proxy(SoupSession *session)
{
	const gchar *proxy;
	gchar *http_uri;
	SoupURI *uri;

	proxy = g_getenv("http_proxy");
	if (!proxy)
		return;

	if (!g_str_has_prefix(proxy, "http://")) {
		http_uri = g_strconcat("http://", proxy, NULL);
		uri = soup_uri_new(http_uri);
		g_free(http_uri);
	} else {
		uri = soup_uri_new(proxy);
	}

	http_uri = soup_uri_to_string(uri, FALSE);
	g_debug("using proxy: %s", http_uri);
	g_free(http_uri);

	g_object_set(session, SOUP_SESSION_PROXY_URI, uri, NULL);
	soup_uri_free(uri);
}
