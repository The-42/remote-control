/*
 * Copyright (C) 2010-2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <glib.h>

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <linux/if_packet.h>

#include "remote-control-stub.h"
#include "remote-control.h"

int32_t medcom_lldp_enable(void *priv, bool enable)
{
	int err = -ENOSYS;
	g_debug("> %s(priv=%p, enable=%d)", __func__, priv, enable);
	g_debug("< %s() = %d", __func__, err);
	return err;
}

int32_t medcom_lldp_read(void *priv, uint32_t mode, struct rpc_buffer *buffer)
{
	struct lldp_monitor *lldp = remote_control_get_lldp_monitor(priv);
	int32_t err = -ENOSYS;

	g_debug("> %s(priv=%p, mode=%#x, buffer=%p)", __func__, priv, mode,
			buffer);
	g_debug("  buffer:");
	g_debug("    rx: %p (%zu bytes)", buffer->rx_buf, buffer->rx_num);
	g_debug("    tx: %p (%zu bytes)", buffer->tx_buf, buffer->tx_num);

	err = lldp_monitor_read(lldp, buffer->tx_buf, buffer->tx_num);

	g_debug("< %s() = %d", __func__, err);
	return err;
}
