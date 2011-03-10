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

remote_public
int remote_task_manager_exec(void *priv, const char *command)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int err;

	err = task_manager_exec_stub(rpc, &ret, command);
	if (err < 0)
		return err;

	return ret;
}

remote_public
int remote_task_manager_kill(void *priv, int pid, int sig)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int err;

	err = task_manager_kill_stub(rpc, &ret, pid, sig);
	if (err < 0)
		return err;

	return ret;
}
