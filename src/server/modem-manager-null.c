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

int modem_manager_create(struct modem_manager **managerp, struct rpc_server *server)
{
	return 0;
}

int modem_manager_free(struct modem_manager *manager)
{
	return 0;
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