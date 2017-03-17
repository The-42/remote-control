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

#include <pthread.h>

#include "remote-control-stub.h"
#include "rcrpc-client.h"

struct remote_client {
	pthread_t thread;
	bool done;
};

static void *poll_event_thread(void *context)
{
	struct rpc_client *rpc = rpc_client_from_priv(context);
	struct remote_client *client = context;
	ssize_t ret = 0;

	while (!client->done) {
		ret = rpc_client_poll(rpc, 250);
		if (ret < 0) {
			if (ret != -ETIMEDOUT)
				client->done = true;

			continue;
		}

		ret = rpc_client_process_events(rpc);
		if (ret < 0)
			continue;
	}

	return NULL;
}

remote_public
int remote_init(struct remote_client **clientp, const char *hostname,
		const char *service)
{
	struct rpc_client *rpc = NULL;
	struct remote_client *client;
	struct rpc_host host;
	int err;

	if (!clientp)
		return -EINVAL;

	err = rpc_client_create(&rpc, NULL, sizeof(*client));
	if (err < 0)
		return err;

	if (!hostname)
		hostname = "localhost";

	if (!service)
		service = "7482";

	memset(&host, 0, sizeof(host));
	host.hostname = hostname;
	host.service = service;

	err = rpc_client_connect(rpc, &host);
	if (err < 0) {
		rpc_client_free(rpc);
		return err;
	}

	err = rpc_client_set_event_handler(rpc, rpc_dispatch_event);
	if (err < 0) {
		rpc_client_free(rpc);
		return err;
	}

	client = rpc_client_priv(rpc);
	client->done = false;

	err = pthread_create(&client->thread, NULL, poll_event_thread, client);
	if (err < 0) {
		rpc_client_free(rpc);
		return err;
	}

	*clientp = client;
	return 0;
}

remote_public
int remote_exit(struct remote_client *client)
{
	struct rpc_client *rpc = rpc_client_from_priv(client);

	if (!client)
		return -EINVAL;

	/* stop polling thread */
	client->done = true;
	pthread_join(client->thread, NULL);

	/* cleanup */
	rpc_client_free(rpc);
	return 0;
}
