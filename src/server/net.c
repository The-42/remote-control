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

#include <unistd.h>

#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <glib.h>

#include "remote-control-stub.h"
#include "remote-control.h"

enum net_read_mode {
	NET_READ_MODE_UDP_NONBLOCK = 0x0,
	NET_READ_MODE_UDP = 0x1,
	NET_READ_MODE_HOSTNAME = 0xa,
	NET_READ_MODE_HWADDR = 0xb,
};

enum net_write_mode {
	NET_WRITE_MODE_UDP_NONBLOCK = 0x0,
	NET_WRITE_MODE_UDP = 0x1,
};

static ssize_t gethwaddr(char *hwaddr, size_t len)
{
	struct ifreq req;
	ssize_t err;
	int skt;

	skt = socket(AF_INET, SOCK_DGRAM, 0);
	if (skt < 0)
		return -errno;

	memset(&req, 0, sizeof(req));
	req.ifr_addr.sa_family = AF_INET;
	strncpy(req.ifr_name, "eth0", IFNAMSIZ - 1);

	err = ioctl(skt, SIOCGIFHWADDR, &req);
	if (err < 0) {
		err = -errno;
		close(skt);
		return err;
	}

	err = snprintf(hwaddr, len, "%02x%02x%02x%02x%02x%02x",
			(uint8_t)req.ifr_hwaddr.sa_data[0],
			(uint8_t)req.ifr_hwaddr.sa_data[1],
			(uint8_t)req.ifr_hwaddr.sa_data[2],
			(uint8_t)req.ifr_hwaddr.sa_data[3],
			(uint8_t)req.ifr_hwaddr.sa_data[4],
			(uint8_t)req.ifr_hwaddr.sa_data[5]);

	close(skt);
	return err;
}

int32_t RPC_IMPL(net_config)(void *priv, uint32_t port, uint32_t timeout, uint32_t repeat, const char *host)
{
	int ret = -ENOSYS;
	g_debug("> %s(priv=%p, port=%u, timeout=%u, repeat=%u, host=%s)",
			__func__, priv, port, timeout, repeat, host);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(net_read)(void *priv, uint32_t mode, struct rpc_buffer *buffer)
{
	struct net *net = remote_control_get_net(priv);
	int ret = -ENOSYS;

	g_debug("> %s(priv=%p, mode=%#x, buffer=%p)", __func__, priv, mode,
			buffer);

	switch (mode) {
	case NET_READ_MODE_UDP_NONBLOCK:
		g_debug("  not implemented: NET_READ_MODE_UDP_NONBOLCK");
		break;

	case NET_READ_MODE_UDP:
		ret = net_recv_sync(net, buffer->tx_buf, buffer->tx_num);
		break;

	case NET_READ_MODE_HOSTNAME:
		gethostname(buffer->tx_buf, buffer->tx_num);
		ret = strlen(buffer->tx_buf);
		buffer->tx_num = ret;
		break;

	case NET_READ_MODE_HWADDR:
		ret = gethwaddr(buffer->tx_buf, buffer->tx_num);
		buffer->tx_num = ret;
		break;
	}

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(net_write)(void *priv, uint32_t mode, struct rpc_buffer *buffer)
{
	struct net *net = remote_control_get_net(priv);
	int ret = -ENOSYS;

	g_debug("> %s(priv=%p, mode=%#x, buffer=%p)", __func__, priv, mode,
			buffer);

	switch (mode) {
	case NET_WRITE_MODE_UDP_NONBLOCK:
		g_debug("  not implemented: NET_WRITE_MODE_UDP_NONBLOCK");
		break;

	case NET_WRITE_MODE_UDP:
		ret = net_send_sync(net, buffer->rx_buf, buffer->rx_num);
		break;
	}

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}
