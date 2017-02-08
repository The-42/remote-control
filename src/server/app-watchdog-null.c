/*
 * Copyright (C) 2015 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "remote-control.h"

struct app_watchdog {
	void *dummy;
};

int app_watchdog_start(struct app_watchdog *watchdog, int interval)
{
	return -ENOSYS;
}

int app_watchdog_stop(struct app_watchdog *watchdog)
{
	return -ENOSYS;
}

int app_watchdog_trigger(struct app_watchdog *watchdog)
{
	return -ENOSYS;
}

int app_watchdog_create(struct app_watchdog **watchdogp, GKeyFile *config)
{
	return 0;
}

int app_watchdog_free(struct app_watchdog *watchdog)
{
	return 0;
}
