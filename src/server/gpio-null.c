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

#include "remote-control-stub.h"
#include "remote-control.h"

static gboolean gpio_source_prepare(GSource *source, gint *timeout)
{
	if (timeout)
		*timeout = -1;

	return FALSE;
}

static gboolean gpio_source_check(GSource *source)
{
	return FALSE;
}

static gboolean gpio_source_dispatch(GSource *source, GSourceFunc callback, gpointer user_data)
{
	if (callback)
		return callback(user_data);

	return TRUE;
}

static void gpio_source_finalize(GSource *source)
{
}

static GSourceFuncs gpio_source_funcs = {
	.prepare = gpio_source_prepare,
	.check = gpio_source_check,
	.dispatch = gpio_source_dispatch,
	.finalize = gpio_source_finalize,
};

GSource *gpio_source_new(struct event_manager *events)
{
	return g_source_new(&gpio_source_funcs, sizeof(GSource));
}
