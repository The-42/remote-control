/*
 * Copyright (C) 2016 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include "remote-control.h"

struct netcb_data {
	int ref;
};

static void recv_callback(int ref, void *data)
{
	struct netcb_data *ncbd = data;

	g_assert_nonnull(ncbd);
	g_assert_cmpint(ref, >=, 0);

	ncbd->ref = ref;
}

/*
 * ping-pong test for the net-udp part of the remote-control lib.
 */
int main(int argc, char **argv)
{
	struct net_udp_channel *secondary = NULL;
	struct net_udp_channel *primary = NULL;
	struct netcb_data netcb_data = { -1 };
	char send_buffer[12] = "test packet";
	char recv_buffer[12] = { 0 };
	struct net_udp *net;
	ssize_t packet_len;
	int cref_pri = 0;
	int cref_sec = 0;
	int owner = 42;
	int ret;
	int i;

	ret = net_udp_create(&net);
	g_assert_cmpint(ret, ==, 0);

	ret = net_udp_set_recv_cb(net, recv_callback, &netcb_data, &owner);
	g_assert_cmpint(ret, ==, 0);
	g_assert_true(net_udp_get_recv_cb_owner(net) == &owner);

	cref_pri = net_udp_create_channel(net, 9998, "localhost", 9999);
	g_assert_cmpint(cref_pri, >, 0);

	cref_sec = net_udp_create_channel(net, 9999, "localhost", 9998);
	g_assert_cmpint(cref_sec, >, 0);

	primary = net_udp_get_channel_by_ref(net, cref_pri);
	g_assert_nonnull(primary);

	secondary = net_udp_get_channel_by_ref(net, cref_sec);
	g_assert_nonnull(secondary);

	packet_len = net_udp_send(primary, send_buffer, sizeof(send_buffer));
	g_assert_cmpint(packet_len, ==, sizeof(send_buffer));

	g_usleep(300000);
	g_assert_cmpint(netcb_data.ref, ==, cref_sec);

	ret = net_udp_recv(secondary, recv_buffer, packet_len);
	g_assert_cmpint(ret, ==, packet_len);
	g_assert_cmpstr(send_buffer, ==, recv_buffer);

	for (i = 0; i < sizeof(recv_buffer); i++)
		recv_buffer[i] = 0;

	packet_len = net_udp_send(secondary, send_buffer, sizeof(send_buffer));
	g_assert_cmpint(packet_len, ==, sizeof(send_buffer));

	g_usleep(300000);
	g_assert_cmpint(netcb_data.ref, ==, cref_pri);

	ret = net_udp_recv(primary, recv_buffer, packet_len);
	g_assert_cmpint(ret, ==, packet_len);
	g_assert_cmpstr(send_buffer, ==, recv_buffer);

	ret = net_udp_destroy_channel(net, cref_pri);
	g_assert_cmpint(ret, ==, 0);

	primary = net_udp_get_channel_by_ref(net, cref_pri);
	g_assert_null(primary);

	ret = net_udp_destroy_channel(net, cref_sec);
	g_assert_cmpint(ret, ==, 0);

	secondary = net_udp_get_channel_by_ref(net, cref_sec);
	g_assert_null(secondary);

	net_udp_free(net);

	return 0;
}
