/*
 * Copyright (C) 2014 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef REMOTE_CONTROL_DATA_H
#define REMOTE_CONTROL_DATA_H 1

#include <glib.h>
#include "remote-control.h"

struct remote_control_data {
	GKeyFile *config;
	GMainLoop *loop;
	GThread *thread;

	GMutex startup_mutex;
	GCond startup_cond;

	struct remote_control *rc;
};

#endif
