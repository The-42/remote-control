/*
 * Copyright (C) 2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef UTILS_H
#define UTILS_H 1

G_BEGIN_DECLS

void soup_session_set_proxy(SoupSession *session);
gchar *soup_session_get_accept_language(SoupSession *session);
void soup_session_set_accept_language(SoupSession *session,
		const gchar *language);

G_END_DECLS

#endif /* UTILS_H */
