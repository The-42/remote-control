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
	int ifindex;
	int sockfd;

	GThread *thread;
	bool done;

	void *data;
	size_t len;
};

static gpointer lldp_thread(gpointer data)
{
	struct lldp_monitor *monitor = data;
	struct timeval timeout;
	fd_set rfds;
	ssize_t err;

	while (!monitor->done) {
		memset(&timeout, 0, sizeof(timeout));
		timeout.tv_sec = 0;
		timeout.tv_usec = 250000;

		FD_ZERO(&rfds);
		FD_SET(monitor->sockfd, &rfds);

		err = select(monitor->sockfd + 1, &rfds, NULL, NULL, &timeout);
		if (err == 0) /* timeout */
			continue;

		err = recv(monitor->sockfd, monitor->data, LLDP_MAX_SIZE, 0);
		if (err <= 0) {
			rc_log(RC_NOTICE "recv(): %s\n", strerror(errno));
			break;
		}

		monitor->len = err;
	}

	return NULL;
}

int lldp_monitor_create(struct lldp_monitor **monitorp)
{
	struct lldp_monitor *monitor;
	struct packet_mreq req;
	struct sockaddr_ll sa;
	int err;

	if (!monitorp)
		return -EINVAL;

	monitor = calloc(1, sizeof(*monitor));
	if (!monitor)
		return -ENOMEM;

	monitor->data = calloc(1, LLDP_MAX_SIZE);
	if (!monitor->data) {
		free(monitor);
		return -ENOMEM;
	}

	monitor->ifindex = if_nametoindex("eth0");
	if (monitor->ifindex == 0) {
		err = -ENODEV;
		goto free;
	}

	err = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_LLDP));
	if (err < 0) {
		err = -errno;
		goto free;
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

	monitor->thread = g_thread_create(lldp_thread, monitor, TRUE, NULL);
	if (!monitor->thread) {
		err = -ENOMEM;
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

	*monitorp = monitor;
	return 0;

close:
	close(monitor->sockfd);
free:
	if (monitor->data)
		free(monitor->data);

	free(monitor);
	return err;
}

int lldp_monitor_free(struct lldp_monitor *monitor)
{
	struct packet_mreq req;
	int err;

	if (!monitor)
		return -EINVAL;

	monitor->done = true;

	if (monitor->sockfd >= 0) {
		memset(&req, 0, sizeof(req));
		req.mr_ifindex = monitor->ifindex;
		req.mr_type = PACKET_MR_MULTICAST;
		req.mr_alen = ETH_ALEN;
		memcpy(req.mr_address, LLDP_MULTICAST_ADDR, ETH_ALEN);

		err = setsockopt(monitor->sockfd, SOL_PACKET,
				PACKET_DROP_MEMBERSHIP, &req, sizeof(req));
		if (err < 0)
			rc_log(RC_NOTICE "setsockopt(): %s\n", strerror(errno));

		close(monitor->sockfd);
	}

	g_thread_join(monitor->thread);

	if (monitor->data)
		free(monitor->data);

	free(monitor);
	return 0;
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
