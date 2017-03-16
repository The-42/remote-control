/*
 * Copyright (C) 2017 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef REMOTE_CONTROL_RPC_H
#define REMOTE_CONTROL_RPC_H 1

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <gtk/gtk.h>
#include <glib.h>

#include <librpc.h>
#include "remote-control.h"

#define BIT(x) (1 << (x))

int rpc_create(void *rcpriv, GKeyFile *config);
int rpc_server_free(struct rpc_server *server);
int remote_control_dispatch(struct rpc_server *server, struct rpc_packet *request);

void rpc_irq_init(void *priv);
void rpc_irq_cleanup(void);

void rpc_net_cleanup(void);

/**
 * Macro dealing with the fact RPC private data now only contains a pointer to
 * remote-control private data.
 */
#define RCPTR(priv)	(priv ? *((struct remote_control **)priv) : NULL)

#endif /* REMOTE_CONTROL_RPC_H */
