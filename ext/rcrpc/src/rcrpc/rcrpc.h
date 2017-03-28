/*
 * Copyright (C) 2017 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef RCRPC_H
#define RCRPC_H 1

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <glib.h>

#include <librpc.h>
#include "remote-control-stub.h"

#define BIT(x) (1 << (x))

void rpc_irq_init(void *priv);
void rpc_irq_cleanup(void);

void rpc_net_cleanup(void);

/**
 * Macro dealing with the fact RPC private data now only contains a pointer to
 * remote-control private data.
 */
#define RCPTR(priv)	(priv ? *((struct remote_control **)priv) : NULL)

#endif /* RCRPC_H */
