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

#ifdef __linux__
#include <sys/socket.h>
#include <linux/if.h>
#else
#include <net/if.h>
#endif
#include <net/if_arp.h>
#include <netlink/route/link.h>
#include <netlink/route/route.h>
#include <netlink/route/rtnl.h>

#include "remote-control.h"

struct nexthop_lookup_result {
	int ifindex;
};

static void nexthop_lookup_cb(struct rtnl_nexthop *nh, void *data)
{
	struct nexthop_lookup_result *result = data;
	result->ifindex = rtnl_route_nh_get_ifindex(nh);
}

struct route_lookup_result {
	struct rtnl_route *route;
	uint32_t priority;
	int ifindex;
};

static void route_lookup_cb(struct nl_object *obj, void *data)
{
	struct rtnl_route *route = (struct rtnl_route *)obj;
	struct route_lookup_result *result = data;
	struct nl_addr *dst;

	dst = rtnl_route_get_dst(route);
	if (!dst || (nl_addr_get_len(dst) == 0)) {
		uint32_t priority;

		priority = rtnl_route_get_priority(route);
		if (priority < result->priority) {
			result->priority = priority;
			result->route = route;
		}
	}
}

struct link_lookup_result {
	unsigned int index;
};

static void link_lookup_cb(struct nl_object *obj, void *data)
{
	struct rtnl_link *link = (struct rtnl_link *)obj;
	struct link_lookup_result *result = data;

	if ((!result->index) && (rtnl_link_get_arptype(link) == ARPHRD_ETHER))
		result->index = rtnl_link_get_ifindex(link);
}

unsigned int if_lookup_default(void)
{
	struct route_lookup_result rlr;
	struct nl_cache *cache = NULL;
	unsigned int retries = 32;
	unsigned int index = 0;
	struct nl_sock *sock;
	unsigned int ret = 0;
	int err;

	sock = nl_socket_alloc();
	if (!sock)
		return 0;

	while (retries--) {
		err = nl_connect(sock, NETLINK_ROUTE);
		if (err < 0) {
			if (err == -NLE_EXIST) {
				g_debug("netlink: reconnecting to ROUTE, %u "
						"retries left", retries);
				nl_socket_set_local_port(sock, 0);
				continue;
			}

			g_debug("netlink: failed to connect to ROUTE: %s",
					nl_geterror(err));
			goto free;
		}

		break;
	}

	err = rtnl_route_alloc_cache(sock, AF_INET, 0, &cache);
	if (err == 0) {
		memset(&rlr, 0, sizeof(rlr));
		rlr.priority = -1U;
		rlr.route = NULL;

		nl_cache_foreach(cache, route_lookup_cb, &rlr);
		if (rlr.route) {
			struct nexthop_lookup_result nlr;

			memset(&nlr, 0, sizeof(nlr));
			nlr.ifindex = 0;

			rtnl_route_foreach_nexthop(rlr.route,
					nexthop_lookup_cb, &nlr);
			index = nlr.ifindex;
		}

		nl_cache_free(cache);
	}

	if (index == 0) {
		struct link_lookup_result result;

		memset(&result, 0, sizeof(result));

		err = rtnl_link_alloc_cache(sock, AF_INET, &cache);
		if (err < 0)
			goto free;

		nl_cache_foreach(cache, link_lookup_cb, &result);
		index = result.index;
		nl_cache_free(cache);
	}

	ret = index;

free:
	nl_socket_free(sock);
	return ret;
}

#if GTK_CHECK_VERSION(3, 20, 0)
void gdk_window_clear(GdkWindow *window)
{
	/* not needed anymore for our use cases */
}
#else
#if GTK_CHECK_VERSION(2, 91, 0)
void gdk_window_clear(GdkWindow *window)
{
	cairo_pattern_t *pattern;
	cairo_t *cr;

	pattern = gdk_window_get_background_pattern(window);
	if (pattern) {
		cr = gdk_cairo_create(window);
		cairo_set_source(cr, pattern);
		cairo_paint(cr);
		cairo_destroy(cr);
	}
}
#endif
#endif
