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

#include "remote-control.h"

int voip_create(struct voip **voipp, struct remote_control *rc,
		GKeyFile *config)
{
	return 0;
}

int voip_free(struct voip *voip)
{
	return 0;
}

GSource *voip_get_source(struct voip *voip)
{
	return NULL;
}

int voip_login(struct voip *voip, const char *host, uint16_t port,
	       const char *username, const char *password,
	       enum voip_transport transport)
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

int voip_get_login_state(struct voip *voip, enum voip_state *statep)
{
	return -ENOSYS;
}

int voip_get_contact(struct voip *voip, const char **namep, const char **displayp)
{
	return -ENOSYS;
}

int voip_dial(struct voip *voip, uint8_t dtmf)
{
	return -ENOSYS;
}

int voip_set_playback(struct voip *voip, const char *card)
{
	return -ENOSYS;
}

int voip_set_capture(struct voip *voip, const char *card)
{
	return -ENOSYS;
}

int voip_set_capture_gain(struct voip *voip, float gain)
{
	return -ENOSYS;
}

int voip_set_onstatechange_cb(struct voip *voip, voip_onstatechange_cb *cb,
			      void *cb_data, void *owner_ref)
{
	return -ENOSYS;
}

void *voip_get_onstatechange_cb_owner(struct voip *voip)
{
	return NULL;
}
