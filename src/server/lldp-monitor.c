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

#include <ctype.h>
#include <glib.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#ifdef __linux__
#include <linux/if.h>
#else
#include <net/if.h>
#endif
#include <linux/if_packet.h>

#include <netlink/netlink.h>
#include <netlink/route/link.h>

#include "remote-control-stub.h"
#include "remote-control.h"

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a))/(sizeof((a)[0]))
#endif

#if defined(__linux__)
extern unsigned int if_nametoindex (const char *__ifname) __THROW;
extern char *if_indextoname (unsigned int __ifindex, char *__ifname) __THROW;
#if !defined(IF_NAMESIZE)
#define IF_NAMESIZE 32
#endif
#endif

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

int lldp_monitor_create(struct lldp_monitor **monitorp, GKeyFile *config)
{
	struct lldp_monitor *monitor;
	char ifname[IF_NAMESIZE];
	struct packet_mreq req;
	struct sockaddr_ll sa;
	GSource *source;
	gchar *value;
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

	value = g_key_file_get_string(config, "lldp", "interface", NULL);
	if (value) {
		g_debug("lldp: requested %s", value);
		monitor->ifindex = if_nametoindex(value);
		g_free(value);
	}

	if (monitor->ifindex == 0) {
		monitor->ifindex = if_lookup_default();
		if (monitor->ifindex == 0) {
			err = -ENODEV;
			goto freedata;
		}
	}

	if_indextoname(monitor->ifindex, ifname);
	g_debug("lldp: using %s", ifname);

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

int lldp_monitor_free(struct lldp_monitor *monitor)
{
	/* Done via source */
	return 0;
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

int lldp_monitor_read_info(struct lldp_monitor *monitor, GHashTable **data)
{
	return -ENOSYS;
}
