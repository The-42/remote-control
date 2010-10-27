#include <netdb.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "remote-control.h"

static bool done = false;

struct medcom {
};

static const char *sockaddr_format(char *buf, size_t size, const struct sockaddr *addr, size_t len)
{
	int err = getnameinfo(addr, size, buf, len, NULL, 0, NI_NUMERICHOST);
	if (err)
		return NULL;

	return buf;
}

static void handle_signal(int signum)
{
	done = true;
}

int main(int argc, char *argv[])
{
	struct rpc_packet *request = NULL;
	struct rpc_server *server = NULL;
	struct medcom *medcom = NULL;
	struct sigaction sa;
	int ret = 0;
	int err;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = handle_signal;

	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	err = rpc_server_create(&server, sizeof(*medcom));
	if (err < 0) {
		fprintf(stderr, "rpc_server_create(): %s\n", strerror(-err));
		ret = -err;
		goto out;
	}

	err = rpc_server_listen(server, 7482);
	if (err < 0) {
		fprintf(stderr, "rpc_server_listen(): %s\n", strerror(-err));
		ret = -err;
		goto out;
	}

	while (!done) {
		struct sockaddr *addr = NULL;
		char buf[NI_MAXHOST + 1];
		const char *peer = NULL;
		int len;

		err = rpc_server_accept(server);
		if (err < 0) {
			fprintf(stderr, "rpc_server_accept(): %s\n", strerror(-err));
			ret = -err;
			continue;
		}

		len = rpc_server_get_peer(server, &addr);
		if ((len > 0) && addr) {
			peer = sockaddr_format(buf, sizeof(buf), addr, len);
			free(addr);
		}

		fprintf(stderr, "connection accepted from %s\n", peer);

		while (!done) {
			err = rpc_server_recv(server, &request);
			if (err < 0) {
				fprintf(stderr, "rpc_server_recv(): %s\n", strerror(-err));
				ret = -err;
				goto out;
			}

			if (err == 0) {
				fprintf(stderr, "connection closed by %s\n", peer);
				break;
			}

			err = rpc_dispatch(server, request);
			if (err < 0) {
				fprintf(stderr, "rpc_dispatch(): %s\n", strerror(-err));
				ret = -err;
				goto out;
			}

			rpc_packet_free(request);
			request = NULL;
		}
	}

	ret = 0;

out:
	rpc_packet_free(request);
	rpc_server_free(server);
	return ret;
}
