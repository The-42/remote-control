/*
 * Copyright (C) 2010-2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <ctype.h>
#include <glib.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <linux/if_packet.h>

#include "remote-control-stub.h"
#include "remote-control.h"

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

#define LLDP_MAX_SIZE 1536

static const uint8_t LLDP_MULTICAST_ADDR[] = { 0x01, 0x80, 0xc2, 0x00, 0x00, 0x0e };
static const uint16_t ETH_P_LLDP = 0x88cc;

struct lldp_monitor {
	GSource source;

	int ifindex;
	int sockfd;
	GPollFD fd;

	void *data;
	size_t len;
};

static gboolean lldp_monitor_source_prepare(GSource *source, gint *timeout)
{
	if (timeout)
		*timeout = -1;

	return FALSE;
}

static gboolean lldp_monitor_source_check(GSource *source)
{
	struct lldp_monitor *monitor = (struct lldp_monitor *)source;

	if (monitor->fd.revents & G_IO_IN)
		return TRUE;

	return FALSE;
}

static gboolean lldp_monitor_source_dispatch(GSource *source, GSourceFunc callback, gpointer user_data)
{
	struct lldp_monitor *monitor = (struct lldp_monitor *)source;
	ssize_t err;

	err = recv(monitor->sockfd, monitor->data, LLDP_MAX_SIZE, 0);
	if (err <= 0) {
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_ERROR, "failed to receive "
				"LLDP frame: %s", strerror(errno));
		monitor->len = 0;
	} else {
		monitor->len = err;
	}

	return TRUE;
}

static void lldp_monitor_source_finalize(GSource *source)
{
	struct lldp_monitor *monitor = (struct lldp_monitor *)source;
	struct packet_mreq req;
	int err;

	if (monitor->sockfd >= 0) {
		memset(&req, 0, sizeof(req));
		req.mr_ifindex = monitor->ifindex;
		req.mr_type = PACKET_MR_MULTICAST;
		req.mr_alen = ETH_ALEN;
		memcpy(req.mr_address, LLDP_MULTICAST_ADDR, ETH_ALEN);

		err = setsockopt(monitor->sockfd, SOL_PACKET,
				PACKET_DROP_MEMBERSHIP, &req, sizeof(req));
		if (err < 0) {
			g_log(G_LOG_DOMAIN, G_LOG_LEVEL_ERROR, "failed to "
					"leave multicast group: %s",
					strerror(errno));
		}

		close(monitor->sockfd);
	}

	g_free(monitor->data);
}

static GSourceFuncs lldp_monitor_source_funcs = {
	.prepare = lldp_monitor_source_prepare,
	.check = lldp_monitor_source_check,
	.dispatch = lldp_monitor_source_dispatch,
	.finalize = lldp_monitor_source_finalize,
};

int lldp_monitor_create(struct lldp_monitor **monitorp)
{
	struct lldp_monitor *monitor;
	struct packet_mreq req;
	struct sockaddr_ll sa;
	GSource *source;
	int err;

	if (!monitorp)
		return -EINVAL;

	source = g_source_new(&lldp_monitor_source_funcs, sizeof(*monitor));
	if (!source)
		return -ENOMEM;

	monitor = (struct lldp_monitor *)source;

	monitor->data = g_malloc0(LLDP_MAX_SIZE);
	if (!monitor->data) {
		err = -ENOMEM;
		goto free;
	}

	monitor->ifindex = if_nametoindex("eth0");
	if (monitor->ifindex == 0) {
		err = -ENODEV;
		goto freedata;
	}

	err = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_LLDP));
	if (err < 0) {
		err = -errno;
		goto freedata;
	}

	monitor->sockfd = err;

	memset(&sa, 0, sizeof(sa));
	sa.sll_family = AF_PACKET;
	sa.sll_protocol = htons(ETH_P_LLDP);
	sa.sll_ifindex = monitor->ifindex;

	err = bind(monitor->sockfd, (struct sockaddr *)&sa, sizeof(sa));
	if (err < 0) {
		err = -errno;
		goto close;
	}

	memset(&req, 0, sizeof(req));
	req.mr_ifindex = monitor->ifindex;
	req.mr_type = PACKET_MR_MULTICAST;
	req.mr_alen = ETH_ALEN;
	memcpy(req.mr_address, LLDP_MULTICAST_ADDR, ETH_ALEN);

	err = setsockopt(monitor->sockfd, SOL_PACKET, PACKET_ADD_MEMBERSHIP,
			&req, sizeof(req));
	if (err < 0) {
		err = -errno;
		goto close;
	}

	monitor->fd.events = G_IO_IN | G_IO_HUP | G_IO_ERR;
	monitor->fd.fd = monitor->sockfd;

	g_source_add_poll(source, &monitor->fd);

	*monitorp = monitor;
	return 0;

close:
	close(monitor->sockfd);
freedata:
	g_free(monitor->data);
free:
	g_free(source);
	return err;
}

GSource *lldp_monitor_get_source(struct lldp_monitor *monitor)
{
	return monitor ? &monitor->source : NULL;
}

ssize_t lldp_monitor_read(struct lldp_monitor *monitor, void *buffer,
		size_t size)
{
	ssize_t ret = 0;

	if (!monitor || !buffer || !size)
		return -EINVAL;

	if (monitor->len) {
		size_t len = min(size, monitor->len);
		memcpy(buffer, monitor->data, len);
		ret = len;
	}

	return ret;
}
