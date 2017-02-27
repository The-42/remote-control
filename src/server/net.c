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
#include <fcntl.h>
#include <time.h>

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

struct net_data {
	struct net_udp *net;
	int primary;
	int secondary;

	uint16_t sync_timeout_msec;
};

static struct net_data *net_data;

static ssize_t gethwaddr(char *hwaddr, size_t len)
{
	char ifname[IF_NAMESIZE];
	struct ifreq req;
	int ifindex;
	ssize_t err;
	int skt;

	skt = socket(AF_INET, SOCK_DGRAM, 0);
	if (skt < 0)
		return -errno;

	memset(&req, 0, sizeof(req));
	req.ifr_addr.sa_family = AF_INET;
	ifindex = if_lookup_default();
	if (ifindex == 0)
		return -ENODEV;
	if (!if_indextoname(ifindex, ifname))
		return -errno;
	g_debug("%s: using MAC from %s", __func__, ifname);
	strncpy(req.ifr_name, ifname, IFNAMSIZ - 1);

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

void rpc_net_cleanup(void)
{
	if (net_data) {
		net_udp_destroy_channel(net_data->net, net_data->primary);
		net_udp_destroy_channel(net_data->net, net_data->secondary);
		g_free(net_data);
		net_data = NULL;
	}
}

int32_t RPC_IMPL(net_config)(void *priv, uint32_t port, uint32_t timeout, uint32_t repeat, const char *host)
{
	struct net_udp *net = remote_control_get_net_udp(priv);
	int ret;

	g_debug("> %s(priv=%p, port=%u, timeout=%u, repeat=%u, host=%s)",
			__func__, priv, port, timeout, repeat, host);

	rpc_net_cleanup();
	net_data = g_new0(struct net_data, 1);
	net_data->net = net;
	net_data->sync_timeout_msec = timeout & 0xFFFF;
	ret = net_udp_create_channel(net, 9999, host, port);
	if (ret >= 0) {
		net_data->primary = ret;
		ret = net_udp_create_channel(net, 49999, "255.255.255.255",
			50000);
		if (ret >= 0) {
			net_data->secondary = ret;
			ret = 0;
		}
	}

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(net_read)(void *priv, uint32_t mode, struct rpc_buffer *buffer)
{
	struct net_udp *net = remote_control_get_net_udp(priv);
	struct net_udp_channel *async_chan;
	struct net_udp_channel *sync_chan;
	int32_t timeout;
	int ret = -ENOSYS;

	g_debug("> %s(priv=%p, mode=%#x, buffer=%p)", __func__, priv, mode,
			buffer);

	async_chan = net_udp_get_channel_by_ref(net, net_data->primary);
	sync_chan = net_udp_get_channel_by_ref(net, net_data->secondary);

	switch (mode) {
	case NET_READ_MODE_UDP_NONBLOCK:
		ret = net_udp_recv(async_chan, buffer->tx_buf, buffer->tx_num);
		break;

	case NET_READ_MODE_UDP:
		/*
		 * HACK: Simulate synchronous reading by sleeping until either a
		 * packet has arrived or time's up. It ain't pretty, but in this
		 * context synchronous read / writes shouldn't be used anyway as
		 * it stalls the process completely. Note that a single packet
		 * is stored, not the actual amount of bytes requested - but
		 * that wasn't different in the old implementation.
		 */
		timeout = net_data->sync_timeout_msec;
		ret = 0;
		while (!ret) {
			ret = net_udp_recv(sync_chan, buffer->tx_buf,
				buffer->tx_num);
			if (ret)
				break;

			usleep(5000);
			timeout -= 5;
			if (timeout < 0)
				ret = -ETIMEDOUT;
		}
		break;

	case NET_READ_MODE_HOSTNAME:
		gethostname(buffer->tx_buf, buffer->tx_num);
		ret = strlen(buffer->tx_buf);
		buffer->tx_num = ret;
		break;

	case NET_READ_MODE_HWADDR:
		ret = gethwaddr(buffer->tx_buf, buffer->tx_num);
		buffer->tx_num = ret > 0 ? ret : 0;
		break;
	}

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(net_write)(void *priv, uint32_t mode, struct rpc_buffer *buffer)
{
	struct net_udp *net = remote_control_get_net_udp(priv);
	struct net_udp_channel *async_chan;
	struct net_udp_channel *sync_chan;
	struct timeval timeout;
	int ret = -ENOSYS;
	fd_set rfds;

	g_debug("> %s(priv=%p, mode=%#x, buffer=%p)", __func__, priv, mode,
			buffer);

	async_chan = net_udp_get_channel_by_ref(net, net_data->primary);
	sync_chan = net_udp_get_channel_by_ref(net, net_data->secondary);

	switch (mode) {
	case NET_WRITE_MODE_UDP_NONBLOCK:
		ret = net_udp_send(async_chan, buffer->rx_buf, buffer->rx_num);
		break;

	case NET_WRITE_MODE_UDP:
		ret = net_udp_send(sync_chan, buffer->rx_buf, buffer->rx_num);
		if (ret < 0)
			break;

		/*
		 * HACK: Simulate blocking operation by watching the channel's
		 * file descriptor. If it can be read after writing, things are
		 * assumed to be synchronized on this strange multicast
		 * endpoint.
		 *
		 * Here be dragons.
		 */
		timeout.tv_sec = net_data->sync_timeout_msec / 1000;
		timeout.tv_usec = (net_data->sync_timeout_msec % 1000) * 1000;

		FD_ZERO(&rfds);
		FD_SET(net_data->secondary, &rfds);

		ret = select(net_data->secondary + 1, &rfds, NULL, NULL,
			&timeout);
		if (ret < 0)
			ret = -errno;
		else if (ret == 0)
			ret = -ETIMEDOUT;
		break;
	}

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}
