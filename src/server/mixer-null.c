/*
 * Copyright (C) 2010-2012 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "remote-control.h"

struct mixer {};

int mixer_create(struct mixer **mixerp)
{
	return 0;
}

GSource *mixer_get_source(struct mixer *mixer)
{
	return NULL;
}

int mixer_set_volume(struct mixer *mixer, unsigned short control, unsigned int volume)
{
	return -ENOSYS;
}

int mixer_get_volume(struct mixer *mixer, unsigned short control, unsigned int *volumep)
{
	return -ENOSYS;
}

int mixer_set_mute(struct mixer *mixer, unsigned short control, bool mute)
{
	return -ENOSYS;
}

int mixer_is_muted(struct mixer *mixer, unsigned short control, bool *mutep)
{
	return -ENOSYS;
}

int mixer_set_input_source(struct mixer *mixer, enum mixer_input_source source)
{
	return -ENOSYS;
}

int mixer_get_input_source(struct mixer *mixer, enum mixer_input_source *sourcep)
{
	return -ENOSYS;
}

int mixer_loopback_enable(struct mixer *mixer, bool enable)
{
	return -ENOSYS;
}

int mixer_loopback_is_enabled(struct mixer *mixer, bool *enabled)
{
	return -ENOSYS;
}
