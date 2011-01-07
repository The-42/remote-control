/*
 * Copyright (C) 2010-2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <pthread.h>

#include "remote-control.h"

struct medcom_client {
	unsigned int done;
	pthread_t thread;
};

static void *poll_event_thread(void *context)
{
	struct rpc_client *rpc = rpc_client_from_priv(context);
	struct medcom_client *client = context;
	ssize_t ret = 0;

	while (!client->done) {
		ret = rpc_client_poll(rpc, 250);
		if (ret < 0) {
			if (ret != -ETIMEDOUT)
				client->done = 1;

			continue;
		}

		ret = rpc_client_process_events(rpc);
		if (ret < 0)
			continue;
	}

	return NULL;
}

int medcom_init(struct medcom_client **clientp, const char *hostname,
		const char *service)
{
	struct rpc_client *rpc = NULL;
	struct medcom_client *client;
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

	err = pthread_create(&client->thread, NULL, poll_event_thread, client);
	if (err < 0) {
		rpc_client_free(rpc);
		return err;
	}

	*clientp = client;
	return 0;
}

int medcom_exit(struct medcom_client *client)
{
	struct rpc_client *rpc = rpc_client_from_priv(client);

	if (!client)
		return -EINVAL;

	/* stop polling thread */
	client->done = 1;
	pthread_join(client->thread, NULL);

	/* cleanup */
	rpc_client_free(rpc);
	return 0;
}
