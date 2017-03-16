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

#include <fcntl.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <glib.h>

#include "remote-control.h"

#define MAX_PACKET_SIZE 1536

struct net_udp_packet {
	void *data;
	size_t len;
};

struct net_udp_channel {
	struct sockaddr *addr;
	socklen_t addrlen;

	/* socket file descriptor */
	int fd;

	/* queue of received packets */
	GQueue *packets;
	size_t buffer_size;
	void *buffer;

	/* Callback for packet received events */
	net_udp_recv_cb recv_cb;
	void *callback_data;
};

struct net_udp {
	/* Configured endpoints */
	GList *channels;
	GPollFD *fds;

	GMutex fds_mutex;

	/* Thread to receive packets */
	GThread *thread;
	gboolean done;
};

static bool addr_is_broadcast(struct sockaddr *addr, socklen_t addrlen)
{
	struct sockaddr_in *sin = (struct sockaddr_in *)addr;

	if (addr->sa_family != AF_INET)
		return false;

	return sin->sin_addr.s_addr == INADDR_BROADCAST;
}

static int net_packet_create(struct net_udp_packet **packetp, const void *data,
		size_t len)
{
	struct net_udp_packet *packet;

	if (!data || !len)
		return -EINVAL;

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

static void net_packet_free(struct net_udp_packet *packet)
{
	if (!packet)
		return;

	free(packet->data);
	free(packet);
}

static int net_channel_create(struct net_udp_channel **channelp,
		const char *hostname, uint16_t local_port,
		uint16_t remote_port)
{
	const unsigned int flags = O_NONBLOCK;
	struct addrinfo *result = NULL;
	struct net_udp_channel *channel;
	static const int yes = 1;
	struct sockaddr_in *addr;
	struct sockaddr_in sin;
	struct addrinfo hints;
	int err;

	channel = calloc(1, sizeof(*channel));
	if (!channel)
		return -ENOMEM;

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

static void net_udp_channel_free(gpointer data)
{
	struct net_udp_channel *channel = data;

	if (!channel)
		return;

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
}

static gint chan_by_ref(gconstpointer item, gconstpointer data)
{
	if (((struct net_udp_channel *)item)->fd == *((int *)data))
		return 0;

	return -1;
}

struct net_udp_channel *net_udp_get_channel_by_ref(struct net_udp *net, int ref)
{
	GList *item;
	if (!net || ref < 0)
		return NULL;

	item = g_list_find_custom(net->channels, &ref, chan_by_ref);

	if (item)
		return (struct net_udp_channel *)(item->data);

	return NULL;
}

static int update_fds(struct net_udp *net)
{
	struct net_udp_channel *channel;
	unsigned int i;
	GList *item;

	if (!net)
		return -EINVAL;

	g_free(net->fds);
	net->fds = g_new0(GPollFD, g_list_length(net->channels));

	item = net->channels;

	for (i = 0; i < g_list_length(net->channels); i++) {
		channel = (struct net_udp_channel *)item->data;
		net->fds[i].fd = channel->fd;
		net->fds[i].events = G_IO_IN;
		item = item->next;
		if (!item)
			break;
	}

	return 0;
}

int net_udp_create_channel(struct net_udp *net, uint16_t local_port,
	const char *hostname, uint16_t remote_port)
{
	struct net_udp_channel *channel = NULL;
	int err;

	if (!net)
		return -EINVAL;

	err = net_channel_create(&channel, hostname, local_port, remote_port);
	if (err < 0)
		return err;

	g_mutex_lock(&net->fds_mutex);
	net->channels = g_list_append(net->channels, channel);
	err = update_fds(net);
	g_mutex_unlock(&net->fds_mutex);

	if (err)
		g_warning("%s: fds update: %s", __func__, g_strerror(err));

	return channel->fd;
}

int net_udp_destroy_channel(struct net_udp *net, int ref)
{
	GList *item;
	int err;

	if (!net || ref < 0)
		return -EINVAL;

	item = g_list_find_custom(net->channels, &ref, chan_by_ref);
	if (!item)
		return 0;

	g_mutex_lock(&net->fds_mutex);
	net->channels = g_list_remove_link(net->channels, item);
	err = update_fds(net);
	g_mutex_unlock(&net->fds_mutex);

	if (err)
		g_warning("%s: fds update: %s", __func__, g_strerror(err));

	net_udp_channel_free(item->data);
	g_list_free(item);

	return 0;
}

static gpointer recv_thread(gpointer context)
{
	struct net_udp_channel *channel;
	struct net_udp *net = context;
	struct net_udp_packet *packet;
	unsigned int i;
	size_t size;
	ssize_t err;
	guint num;

	while (!net->done) {
		/* Important: sleep a bit so others get a chance to lock */
		g_usleep(500);
		g_mutex_lock(&net->fds_mutex);
		num = g_list_length(net->channels);
		if (!num) {
			g_mutex_unlock(&net->fds_mutex);
			g_usleep(250000);
			continue;
		}

		err = g_poll(net->fds, num, 500);
		if (err <= 0) {
			if ((err == 0) || (errno == EINTR)) {
				g_mutex_unlock(&net->fds_mutex);
				continue;
			}

			g_warning("%s(): g_poll(): %s", __func__,
				g_strerror(errno));
			g_mutex_unlock(&net->fds_mutex);
			break;
		}


		for (i = 0; i < num; i++) {
			channel = net_udp_get_channel_by_ref(net,
				net->fds[i].fd);
			if (!channel) {
				g_warning("%s(): fd %d: packet lost!", __func__,
					net->fds[i].fd);
				continue;
			}
			if (!(net->fds[i].revents & G_IO_IN))
				continue;

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

			if (channel->recv_cb) {
				channel->recv_cb(channel,
					channel->callback_data);
			}
		}
		g_mutex_unlock(&net->fds_mutex);
	}

	return NULL;
}

int net_udp_create(struct net_udp **netp)
{
	struct net_udp *net;

	if (!netp)
		return -EINVAL;

	net = calloc(1, sizeof(*net));
	if (!net)
		return -ENOMEM;

	*netp = net;

	net->thread = g_thread_new("net-udp", recv_thread, net);
	if (!net->thread) {
		net_udp_free(net);
		return -ENOMEM;
	}

	g_mutex_init(&net->fds_mutex);

	return 0;
}

void net_udp_free(struct net_udp *net_udp)
{
	if (!net_udp)
		return;

	net_udp->done = TRUE;
	g_thread_join(net_udp->thread);

	g_mutex_clear(&net_udp->fds_mutex);

	g_list_free_full(net_udp->channels, net_udp_channel_free);
	free(net_udp);
}

ssize_t net_udp_send(struct net_udp_channel *channel, const void *buffer,
		size_t size)
{
	ssize_t ret;

	if (!channel || !buffer || !size)
		return -EINVAL;

	ret = sendto(channel->fd, buffer, size, 0, channel->addr,
			channel->addrlen);
	if (ret < 0)
		ret = -errno;

	return ret;
}

ssize_t net_udp_recv(struct net_udp_channel *channel, void *buffer, size_t size)
{
	struct net_udp_packet *packet;
	size_t count = size;
	ssize_t ret = 0;

	if (!channel || !buffer || !size)
		return -EINVAL;

	packet = g_queue_pop_head(channel->packets);
	if (packet) {
		if (count > packet->len)
			count = packet->len;
		memcpy(buffer, packet->data, count);
		net_packet_free(packet);
		ret = count;
	}

	return ret;
}

int net_udp_set_recv_cb(struct net_udp_channel *channel, net_udp_recv_cb cb,
			void *cb_data)
{
	if (!channel)
		return -EINVAL;

	channel->recv_cb = cb;
	channel->callback_data = cb_data;

	return 0;
}
