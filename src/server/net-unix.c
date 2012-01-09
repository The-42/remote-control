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

#include <fcntl.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <glib.h>

#include "remote-control-stub.h"
#include "remote-control.h"

#define min(a, b) ((a < b) ? (a) : (b))
#define MAX_PACKET_SIZE 1536

static bool addr_is_broadcast(struct sockaddr *addr, socklen_t addrlen)
{
	struct sockaddr_in *sin = (struct sockaddr_in *)addr;

	if (addr->sa_family != AF_INET)
		return false;

	return sin->sin_addr.s_addr == INADDR_BROADCAST;
}

struct net_packet {
	void *data;
	size_t len;
};

static int net_packet_create(struct net_packet **packetp, const void *data,
		size_t len)
{
	struct net_packet *packet;

	packet = malloc(sizeof(*packet));
	if (!packet)
		return -ENOMEM;

	packet->data = malloc(len);
	if (!packet->data) {
		free(packet);
		return -ENOMEM;
	}

	packet->len = len;

	memcpy(packet->data, data, len);

	*packetp = packet;
	return 0;
}

static void net_packet_free(struct net_packet *packet)
{
	if (!packet)
		return;

	free(packet->data);
	free(packet);
}

struct net_channel {
	struct sockaddr *addr;
	socklen_t addrlen;
	unsigned long timeout;
	unsigned int repeat;

	/* socket file descriptor */
	int fd;

	/* queue of received packets */
	GQueue *packets;
	size_t buffer_size;
	void *buffer;

	/* thread to receive packets */
	GThread *thread;
	gboolean done;
};

static int net_channel_create(struct net_channel **channelp,
		const char *hostname, uint16_t local_port,
		uint16_t remote_port, unsigned long timeout,
		unsigned int repeat, unsigned int flags)
{
	struct addrinfo *result = NULL;
	struct net_channel *channel;
	static const int yes = 1;
	/* TODO: support IPv6? */
	struct sockaddr_in *addr;
	struct sockaddr_in sin;
	struct addrinfo hints;
	int err;

	channel = calloc(1, sizeof(*channel));
	if (!channel)
		return -ENOMEM;

	/* TODO: support IPv6? */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;

	err = getaddrinfo(hostname, NULL, &hints, &result);
	if (err != 0) {
		g_warning("%s(): getaddrinfo(): %s", __func__,
				gai_strerror(err));
		err = -EIO;
		goto free;
	}

	channel->fd = socket(result->ai_family, result->ai_socktype,
			result->ai_protocol);
	if (channel->fd < 0) {
		err = -errno;
		goto free_ai;
	}

	/* set socket non-blocking */
	if (flags != 0) {
		err = fcntl(channel->fd, F_GETFL, 0);
		if (err < 0) {
			err = -errno;
			goto close;
		}

		err = fcntl(channel->fd, F_SETFL, err | flags);
		if (err < 0) {
			err = -errno;
			goto close;
		}
	}

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(local_port);
	sin.sin_addr.s_addr = INADDR_ANY;

	err = bind(channel->fd, (struct sockaddr *)&sin, sizeof(sin));
	if (err < 0) {
		err = -errno;
		g_warning("%s(): bind(): %s", __func__, strerror(-err));
		goto close;
	}

	channel->addr = malloc(result->ai_addrlen);
	if (!channel->addr) {
		err = -ENOMEM;
		goto close;
	}

	memcpy(channel->addr, result->ai_addr, result->ai_addrlen);
	channel->addrlen = result->ai_addrlen;
	channel->timeout = timeout;
	channel->repeat = repeat;

	/* TODO: support IPv6? */
	addr = (struct sockaddr_in *)channel->addr;
	addr->sin_port = htons(remote_port);

	if (addr_is_broadcast(channel->addr, channel->addrlen)) {
		err = setsockopt(channel->fd, SOL_SOCKET, SO_BROADCAST, &yes,
				sizeof(yes));
		if (err < 0) {
			err = -errno;
			goto free_ad;
		}
	}

	channel->packets = g_queue_new();
	if (!channel->packets) {
		err = -ENOMEM;
		goto free_ad;
	}

	channel->buffer = malloc(MAX_PACKET_SIZE);
	if (!channel->buffer) {
		err = -ENOMEM;
		goto free_q;
	}

	channel->buffer_size = MAX_PACKET_SIZE;

	freeaddrinfo(result);
	*channelp = channel;
	return 0;

free_q:
	g_queue_free(channel->packets);
free_ad:
	free(channel->addr);
close:
	close(channel->fd);
free_ai:
	freeaddrinfo(result);
free:
	free(channel);
	return err;
}

static void packet_free(gpointer data, gpointer user_data)
{
	net_packet_free(data);
}

static int net_channel_free(struct net_channel *channel)
{
	if (!channel)
		return -EINVAL;

	if (channel->thread) {
		channel->done = TRUE;
		g_thread_join(channel->thread);
	}

	if (channel->buffer)
		free(channel->buffer);

	if (channel->packets) {
		g_queue_foreach(channel->packets, packet_free, NULL);
		g_queue_free(channel->packets);
	}

	if (channel->addr)
		free(channel->addr);

	if (channel->fd >= 0)
		close(channel->fd);

	free(channel);

	return 0;
}

struct net {
	struct rpc_server *server;
	struct net_channel *primary;
	struct net_channel *secondary;
};

int net_create(struct net **netp, struct rpc_server *server)
{
	struct net *net;
	int err = 0;

	if (!netp)
		return -EINVAL;

	net = calloc(1, sizeof(*net));
	if (!net)
		return -ENOMEM;

	net->server = server;

	err = net_channel_create(&net->secondary, "255.255.255.255", 49999,
			50000, 2000, 3, 0);
	if (err < 0) {
		g_warning("failed to create secondary channel: %s",
				g_strerror(-err));
		free(net);
		return err;
	}

	*netp = net;
	return 0;
}

int net_free(struct net *net)
{
	if (!net)
		return -EINVAL;

	net_channel_free(net->secondary);
	net_channel_free(net->primary);
	free(net);

	return 0;
}

static gpointer primary_thread(gpointer context)
{
	struct net_channel *channel;
	struct net *net = context;

	channel = net->primary;

	while (!channel->done) {
		GPollFD fds[1];
		ssize_t err;

		fds[0].fd = channel->fd;
		fds[0].events = G_IO_IN;

		err = g_poll(fds, G_N_ELEMENTS(fds), 2500);
		if (err <= 0) {
			if ((err == 0) || (errno == EINTR))
				continue;

			g_warning("%s(): g_poll(): %s", __func__,
					g_strerror(errno));
			break;
		}

		if (fds[0].revents & G_IO_IN) {
			struct net_packet *packet;
			size_t size;

			err = recv(channel->fd, channel->buffer,
					channel->buffer_size, 0);
			if (err < 0) {
				g_warning("%s(): recv(): %s", __func__,
						g_strerror(errno));
				break;
			}

			size = err;

			err = net_packet_create(&packet, channel->buffer, size);
			if (err < 0) {
				g_warning("%s(): net_packet_create(): %s",
						__func__, g_strerror(-err));
				break;
			}

			g_queue_push_tail(channel->packets, packet);

			err = RPC_STUB(irq_event)(net->server, 1);
			if (err < 0) {
				g_warning("%s(): %s(): %s", __func__,
						G_STRINGIFY(RPC_STUB(irq_event)),
						g_strerror(-err));
				break;
			}
		}
	}

	return NULL;
}

int net_configure(struct net *net, const char *hostname, uint16_t port,
		unsigned long timeout, unsigned int repeat)
{
	struct net_channel *channel = NULL;
	int err;

	g_debug("> %s(net=%p, hostname=\"%s\", port=%u, timeout=%lu, repeat=%u)",
			__func__, net, hostname, port, timeout, repeat);

	if (!net)
		return -EINVAL;

	net_channel_free(net->primary);
	net->primary = NULL;

	err = net_channel_create(&channel, hostname, 9999, port, timeout,
			repeat, O_NONBLOCK);
	if (err < 0)
		return err;

	net->primary = channel;

#if GLIB_CHECK_VERSION(2, 31, 0)
	channel->thread = g_thread_new("net-unix", primary_thread, net);
#else
	channel->thread = g_thread_create(primary_thread, net, TRUE, NULL);
#endif
	if (!channel->thread) {
		net_channel_free(channel);
		return -ENOMEM;
	}

	g_debug("< %s()", __func__);
	return 0;
}

ssize_t net_send_async(struct net *net, const void *buffer, size_t size)
{
	struct net_channel *channel;
	ssize_t err;

	g_debug("> %s(net=%p, buffer=%p, size=%zu)", __func__, net, buffer, size);

	if (!net || !buffer || !size) {
		err = -EINVAL;
		goto out;
	}

	channel = net->primary;

	err = sendto(channel->fd, buffer, size, 0, channel->addr,
			channel->addrlen);
	if (err < 0)
		err = -errno;

out:
	g_debug("< %s() = %zd", __func__, err);
	return err;
}

ssize_t net_send_sync(struct net *net, const void *buffer, size_t size)
{
	struct net_channel *channel;
	struct timeval timeout;
	fd_set rfds;
	ssize_t err;

	g_debug("> %s(net=%p, buffer=%p, size=%zu)", __func__, net, buffer, size);

	if (!net || !buffer || !size) {
		err = -EINVAL;
		goto out;
	}

	channel = net->secondary;

	err = sendto(channel->fd, buffer, size, 0, channel->addr,
			channel->addrlen);
	if (err < 0) {
		err = -errno;
		goto out;
	}

	timeout.tv_sec = channel->timeout / 1000;
	timeout.tv_usec = (channel->timeout % 1000) * 1000;

	FD_ZERO(&rfds);
	FD_SET(channel->fd, &rfds);

	err = select(channel->fd + 1, &rfds, NULL, NULL, &timeout);
	if (err < 0) {
		err = -errno;
		goto out;
	}

	if (err == 0)
		err = -ETIMEDOUT;

out:
	g_debug("< %s() = %zd", __func__, err);
	return err;
}

ssize_t net_recv_async(struct net *net, void *buffer, size_t size)
{
	struct net_channel *channel;
	struct net_packet *packet;
	ssize_t ret = 0;

	g_debug("> %s(net=%p, buffer=%p, size=%zu)", __func__, net, buffer, size);

	if (!net || !buffer || !size) {
		ret = -EINVAL;
		goto out;
	}

	channel = net->primary;

	packet = g_queue_pop_head(channel->packets);
	if (packet) {
		size_t count = min(size, packet->len);
		memcpy(buffer, packet->data, count);
		net_packet_free(packet);
		ret = count;
	}

	if (!g_queue_is_empty(channel->packets)) {
		int err = RPC_STUB(irq_event)(net->server, 1);
		if (err < 0) {
			g_warning("%s(): %s(): %s", __func__,
					G_STRINGIFY(RPC_STUB(irq_event)),
					g_strerror(-err));
		}
	}

out:
	g_debug("< %s() = %zd", __func__, ret);
	return ret;
}

ssize_t net_recv_sync(struct net *net, void *buffer, size_t size)
{
	struct net_channel *channel;
	ssize_t err;

	g_debug("> %s(net=%p, buffer=%p, size=%zu)", __func__, net, buffer, size);

	if (!net || !buffer || !size) {
		err = -EINVAL;
		goto out;
	}

	channel = net->secondary;

	err = recv(channel->fd, buffer, size, 0);
	if (err < 0)
		err = -errno;

out:
	g_debug("< %s() = %zd", __func__, err);
	return err;
}
