/*
 * Copyright (C) 2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef UTILS_H
#define UTILS_H 1

#ifndef USE_WEBKIT2
#include <libsoup/soup-logger.h>
#include <libsoup/soup-session.h>
#endif

G_BEGIN_DECLS

#ifndef USE_WEBKIT2
void soup_session_set_proxy(SoupSession *session);
#endif

struct watchdog;
struct watchdog *watchdog_new(GKeyFile *conf, GError **error);
void watchdog_unref(struct watchdog *watchdog);
void watchdog_attach(struct watchdog *watchdog, GMainContext *context);
/*
 * @watchdog The watchdog to trigger.
 * @message Debug info to have a hint to the last action. The string is
 *          limited to 64 characters.
 */
void watchdog_set_message(struct watchdog *watchdog, const gchar *message);

G_END_DECLS

#endif /* UTILS_H */
