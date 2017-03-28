/*
 * Copyright (C) 2017 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef REMOTE_CONTROL_RPC_H
#define REMOTE_CONTROL_RPC_H 1

#include <glib.h>
#include <librpc.h>

int rcrpc_create(void *rcpriv, GKeyFile *config);

#endif /* REMOTE_CONTROL_RPC_H */
