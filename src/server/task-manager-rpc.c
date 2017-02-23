/*
 * Copyright (C) 2017 Avionic Design GmbH
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

int32_t RPC_IMPL(task_manager_exec)(void *priv, const char *command_line)
{
	return task_manager_exec(priv, command_line, NULL, NULL);
}

int32_t RPC_IMPL(task_manager_kill)(void *priv, int32_t pid, int32_t sig)
{
	return task_manager_kill(priv, pid, sig);
}
