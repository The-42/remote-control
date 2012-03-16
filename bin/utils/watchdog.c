/*
 * Copyright (C) 2012 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>

#ifdef ENABLE_WATCHDOG
#  include <dbus-watchdog.h>
#  include <glib-unix.h>
#endif

#ifdef ENABLE_WATCHDOG
struct watchdog {
	DBusWatchdog *dbus;
	GSource *source;
	gchar *message;
};

static gboolean watchdog_timeout(gpointer data)
{
	const gchar *message = "Hello, World!";
	struct watchdog *watchdog = data;

	if (watchdog->message)
		message = watchdog->message;

	dbus_watchdog_ping(watchdog->dbus, NULL, message);

	return TRUE;
}

static void watchdog_destroy(gpointer data)
{
	struct watchdog *watchdog = data;

	dbus_watchdog_unref(watchdog->dbus);
	g_free(watchdog->message);
	g_free(watchdog);
}

struct watchdog *watchdog_new(GKeyFile *conf, GError **error)
{
	struct watchdog *watchdog;
	guint64 timeout;

	if (!g_key_file_has_key(conf, "watchdog", "timeout", error))
		return NULL;

	timeout = g_key_file_get_uint64(conf, "watchdog", "timeout", error);
	if (!timeout)
		return NULL;

	watchdog = g_new0(struct watchdog, 1);
	if (!watchdog) {
		g_set_error_literal(error, G_UNIX_ERROR, ENOMEM,
				g_strerror(ENOMEM));
		return NULL;
	}

	watchdog->source = g_timeout_source_new_seconds(timeout / 3000);
	if (!watchdog->source) {
		g_set_error_literal(error, G_UNIX_ERROR, ENOMEM,
				g_strerror(ENOMEM));
		g_free(watchdog);
		return NULL;
	}

	watchdog->dbus = dbus_watchdog_new(timeout, NULL);
	if (!watchdog->dbus) {
		g_source_unref(watchdog->source);
		g_free(watchdog);
		return NULL;
	}

	g_source_set_callback(watchdog->source, watchdog_timeout, watchdog,
			watchdog_destroy);

	return watchdog;
}

void watchdog_unref(struct watchdog *watchdog)
{
	if (watchdog && watchdog->source)
		g_source_destroy(watchdog->source);
}

void watchdog_attach(struct watchdog *watchdog, GMainContext *context)
{
	if (watchdog && watchdog->source) {
		g_source_attach(watchdog->source, context);
		g_source_unref(watchdog->source);
	}
}

void watchdog_set_message(struct watchdog *watchdog, const gchar *fmt, ...)
{
	va_list ap;

	if (!watchdog)
		return;

	g_free(watchdog->message);

	va_start(ap, fmt);
	watchdog->message = g_strdup_vprintf(fmt, ap);
	va_end(ap);
}
#else
struct watchdog *watchdog_new(GKeyFile *conf, GError **error)
{
	return NULL;
}

void watchdog_unref(GSource source)
{
}

void watchdog_attach(struct watchdog *watchdog, GMainContext *context)
{
}

void watchdog_set_message(struct watchdog *watchdog, const gchar *fmt, ...)
{
}
#endif
