#include <arpa/inet.h>
#include <netinet/in.h>

#include <glib.h>

#include "remote-control-stub.h"
#include "remote-control.h"

struct net {
	char *hostname;
	uint16_t port;
	unsigned long timeout; /* in milliseconds */
	int fd;
};

int net_create(struct net **netp)
{
	static const int yes = 1;
	struct sockaddr_in sin;
	struct net *net;
	int ret = 0;

	if (!netp)
		return -EINVAL;

	net = malloc(sizeof(*net));
	if (!net)
		return -ENOMEM;

	memset(net, 0, sizeof(*net));

	net->hostname = strdup("255.255.255.255");
	net->port = 50000;
	net->timeout = 1000;

	net->fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (net->fd < 0) {
		ret = -errno;
		net_free(net);
		return ret;
	}

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(49999);
	sin.sin_addr.s_addr = INADDR_ANY;

	ret = bind(net->fd, (struct sockaddr *)&sin, sizeof(sin));
	if (ret < 0) {
		ret = -errno;
		net_free(net);
		return ret;
	}

	ret = setsockopt(net->fd, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
	if (ret < 0) {
		ret = -errno;
		net_free(net);
		return ret;
	}

	*netp = net;
	return 0;
}

int net_free(struct net *net)
{
	if (!net)
		return -EINVAL;

	if (net->fd >= 0)
		close(net->fd);

	if (net->hostname)
		free(net->hostname);

	free(net);
	return 0;
}

ssize_t net_send_sync(struct net *net, const void *buffer, size_t size)
{
	struct sockaddr_in sin;
	struct timeval timeout;
	fd_set rfds;
	ssize_t err;

	g_debug("> %s(net=%p, buffer=%p, size=%zu)", __func__, net, buffer, size);

	if (!net || !buffer || !size) {
		err = -EINVAL;
		goto out;
	}

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(net->port);
	sin.sin_addr.s_addr = inet_addr(net->hostname);

	err = sendto(net->fd, buffer, size, 0, (struct sockaddr *)&sin, sizeof(sin));
	if (err < 0) {
		err = -errno;
		goto out;
	}

	timeout.tv_sec = net->timeout / 1000;
	timeout.tv_usec = (net->timeout % 1000) * 1000;

	FD_ZERO(&rfds);
	FD_SET(net->fd, &rfds);

	err = select(net->fd + 1, &rfds, NULL, NULL, &timeout);
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

ssize_t net_recv_sync(struct net *net, void *buffer, size_t size)
{
	ssize_t err;

	g_debug("> %s(net=%p, buffer=%p, size=%zu)", __func__, net, buffer, size);

	if (!net || !buffer || !size) {
		err = -EINVAL;
		goto out;
	}

	err = recv(net->fd, buffer, size, 0);
	if (err < 0)
		err = -errno;

out:
	g_debug("< %s() = %zd", __func__, err);
	return err;
}
