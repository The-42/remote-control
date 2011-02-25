/*
 * Copyright (C) 2010-2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "remote-control-stub.h"
#include "remote-control.h"

struct voip {
	int foralloc;
};

static gpointer voip_thread(gpointer data)
{
	struct voip *voip = data;

	while (!voip->done) {
		linphone_core_iterate(voip->core);
		g_usleep(50000);
	}

	return NULL;
}

int voip_create(struct voip **voipp, struct rpc_server *server)
{
	struct voip *voip;

	if (!voipp)
		return -EINVAL;

	voip = malloc(sizeof(*voip));
	if (!voip)
		return -ENOMEM;

	memset(voip, 0, sizeof(*voip));

	*voipp = voip;
	return 0;
}

int voip_free(struct voip *voip)
{
	if (!voip)
		return -EINVAL;

	free(voip);
	return 0;
}

int voip_login(struct voip *voip, const char *host, uint16_t port,
		const char *username, const char *password)
{
	return -ENOSYS;
}

int voip_logout(struct voip *voip)
{
	return -ENOSYS;
}

int voip_call(struct voip *voip, const char *uri)
{
	return -ENOSYS;
}

int voip_accept(struct voip *voip, char **caller)
{
	return -ENOSYS;
}

int voip_terminate(struct voip *voip)
{
	return -ENOSYS;
}

int voip_get_state(struct voip *voip, enum voip_state *statep)
{
	return -ENOSYS;
}

int voip_get_contact(struct voip *voip, const char **contactp)
{
	return -ENOSYS;
}
