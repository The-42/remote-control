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

#include <glib.h>

#include "remote-control-stub.h"
#include "remote-control.h"

int32_t RPC_IMPL(rfid_get_type)(void *priv, enum RPC_TYPE(rfid_type) *type)
{
	struct rfid *rfid = remote_control_get_rfid(priv);
	int32_t ret;

	g_debug("> %s(priv=%p, type=%p)", __func__, priv, type);

	ret = rfid_get_type(rfid, type);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(rfid_read)(void *priv, off_t offset, struct rpc_buffer *buffer)
{
	struct rfid *rfid = remote_control_get_rfid(priv);
	int32_t ret;

	g_debug("> %s(priv=%p, offset=%ld, buffer=%p)", __func__, priv, offset,
			buffer);

	ret = rfid_read(rfid, offset, buffer->tx_buf, buffer->tx_num);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(rfid_write)(void *priv, off_t offset,
		struct rpc_buffer *buffer)
{
	struct rfid *rfid = remote_control_get_rfid(priv);
	int32_t ret;

	g_debug("> %s(priv=%p, offset=%ld, buffer=%p)", __func__, priv, offset,
			buffer);

	ret = rfid_write(rfid, offset, buffer->rx_buf, buffer->rx_num);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}
