/*
 * Copyright (C) 2010-2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>

#include "remote-control-stub.h"
#include "remote-control-rpc.h"

int32_t RPC_IMPL(reset)(void *priv)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p)", __func__, priv);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}
