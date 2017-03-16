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

#include <dbus-watchdog.h>

#include "remote-control.h"

struct app_watchdog {
	DBusWatchdog *dbus_watchdog;
	guint interval;
};

int app_watchdog_start(struct app_watchdog *watchdog, int interval)
{
	if (watchdog == NULL)
		return -EINVAL;

	/* keep previous interval if parameter is <= 0 */
	if (interval > 0)
		watchdog->interval = interval;

	if (watchdog->interval <= 0)
		return -EINVAL;

	watchdog->dbus_watchdog = dbus_watchdog_new(watchdog->interval * 1000,
			NULL);
	if (!watchdog->dbus_watchdog)
		return -ENODEV;

	return 0;
}

int app_watchdog_stop(struct app_watchdog *watchdog)
{
	if (watchdog == NULL)
		return -EINVAL;

	if (watchdog->dbus_watchdog != NULL) {
		dbus_watchdog_stop(watchdog->dbus_watchdog, NULL);
		dbus_watchdog_unref(watchdog->dbus_watchdog);
	}

	watchdog->dbus_watchdog = NULL;

	return 0;
}

int app_watchdog_trigger(struct app_watchdog *watchdog)
{
	if (watchdog == NULL)
		return -EINVAL;

	if (watchdog->dbus_watchdog == NULL)
		return -ENODEV;

	if (!dbus_watchdog_ping(watchdog->dbus_watchdog, NULL, "app-watchdog"))
		return -ENODEV;

	return 0;
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

int app_watchdog_free(struct app_watchdog *watchdog)
{
	if (!watchdog)
		return -EINVAL;

	g_free(watchdog);

	return 0;
}
