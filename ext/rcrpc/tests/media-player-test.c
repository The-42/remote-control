#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "rcrpc-client.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

static bool done = false;

static void signal_handler(int signum)
{
	done = true;
}

static void usage(FILE *fp, const char *program)
{
	fprintf(fp, "usage: %s hostname\n", program);
}

int main(int argc, char *argv[])
{
	const char *streams[] = {
		"udp://239.0.21.1:4444",
		"udp://239.0.22.1:4444"
	};
	struct remote_client *client;
	unsigned int stream = 0;
	struct sigaction sa;
	int err;

	if (argc < 2) {
		usage(stderr, argv[0]);
		return 1;
	}

	err = remote_init(&client, argv[1], "7482");
	if (err < 0) {
		fprintf(stderr, "remote_init(): %s\n", strerror(-err));
		return 1;
	}

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = signal_handler;

	err = sigaction(SIGINT, &sa, NULL);
	if (err < 0) {
		fprintf(stderr, "sigaction(): %s\n", strerror(errno));
		remote_exit(client);
		return 1;
	}

	err = remote_media_player_set_output_window(client, 64, 64, 320, 240);
	if (err < 0) {
		fprintf(stderr, "failed to set output window: %s\n",
			strerror(-err));
		remote_exit(client);
		return 1;
	}

	while (!done) {
		struct timespec ts = { 5, 0 };

		err = remote_media_player_set_stream(client, streams[stream]);
		if (err < 0) {
			fprintf(stderr, "failed to set input stream: %s\n",
				strerror(-err));
			remote_exit(client);
			return 1;
		}

		err = remote_media_player_start(client);
		if (err < 0) {
			fprintf(stderr, "failed to start stream: %s\n",
				strerror(-err));
			break;
		}

		err = nanosleep(&ts, NULL);
		if (err < 0) {
			fprintf(stderr, "nanosleep(): %s\n", strerror(errno));
			break;
		}

		err = remote_media_player_stop(client);
		if (err < 0) {
			fprintf(stderr, "failed to stop stream: %s\n",
				strerror(-err));
			break;
		}

		err = nanosleep(&ts, NULL);
		if (err < 0) {
			fprintf(stderr, "nanosleep(): %s\n", strerror(errno));
			break;
		}

		if (++stream >= ARRAY_SIZE(streams))
			stream = 0;
	}

	remote_exit(client);
	return 0;
}
