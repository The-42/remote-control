/*
 * Copyright (C) 2011-2012 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "remote-control.h"

int modem_manager_create(struct modem_manager **managerp,
		struct remote_control *rc, GKeyFile *config)
{
	return 0;
}

GSource *modem_manager_get_source(struct modem_manager *manager)
{
	return NULL;
}

int modem_manager_initialize(struct modem_manager *manager)
{
	return -ENOSYS;
}

int modem_manager_shutdown(struct modem_manager *manager)
{
	return -ENOSYS;
}

int modem_manager_call(struct modem_manager *manager, const char *number)
{
	return -ENOSYS;
}

int modem_manager_accept(struct modem_manager *manager)
{
	return -ENOSYS;
}

int modem_manager_terminate(struct modem_manager *manager)
{
	return -ENOSYS;
}

int modem_manager_get_state(struct modem_manager *manager, enum modem_state *statep)
{
	return -ENOSYS;
}
