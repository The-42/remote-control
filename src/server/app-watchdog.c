/*
 * Copyright (C) 2014 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <errno.h>

#include "remote-control.h"

struct app_watchdog {
	GSource *timeout_source;
	guint interval;
};

static gboolean app_watchdog_timeout(gpointer data)
{
	g_critical("WATCHDOG: It seems the user interface is stalled, restarting");
	raise(SIGTERM);
	return FALSE;
}

int app_watchdog_start(struct app_watchdog *watchdog, int interval)
{
	if (watchdog == NULL)
		return -EINVAL;

	/* keep previous interval if parameter is <= 0 */
	if (interval > 0)
		watchdog->interval = interval;

	if (watchdog->interval <= 0)
		return -EINVAL;

	if (watchdog->timeout_source != NULL)
		g_source_destroy(watchdog->timeout_source);

	watchdog->timeout_source = g_timeout_source_new_seconds(watchdog->interval);
	if (!watchdog->timeout_source)
		return -ENOMEM;

	g_source_set_callback(watchdog->timeout_source, app_watchdog_timeout,
		watchdog, NULL);
	g_source_attach(watchdog->timeout_source, g_main_context_default());

	return 0;
}

int app_watchdog_stop(struct app_watchdog *watchdog)
{
	if (watchdog == NULL)
		return -EINVAL;

	if (watchdog->timeout_source != NULL)
		g_source_destroy(watchdog->timeout_source);

	watchdog->timeout_source = NULL;

	return 0;
}

int app_watchdog_trigger(struct app_watchdog *watchdog)
{
	int ret;
	if (watchdog == NULL)
		return -EINVAL;

	if (watchdog->timeout_source == NULL)
		return -ENODEV;

	return app_watchdog_start(watchdog, 0);
}

int app_watchdog_create(struct app_watchdog **watchdogp, GKeyFile *config)
{
	struct app_watchdog *watchdog;
	int interval;

	watchdog = g_new0(struct app_watchdog, 1);
	if (!watchdog)
		return -ENOMEM;

	interval = g_key_file_get_integer(config, "js-watchdog", "timeout",
			NULL);
	if (interval > 0) {
		g_debug("%s: Autostart watchdog with interval %d", __func__,
				interval);
		if (app_watchdog_start(watchdog, interval) != 0)
			g_warning("%s: Could not autostart watchdog", __func__);
	}

	*watchdogp = watchdog;
	return 0;
}
