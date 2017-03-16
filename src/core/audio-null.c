/*
 * Copyright (C) 2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>

#include "remote-control.h"

int audio_create(struct audio **audiop, struct remote_control *rc, GKeyFile *config)
{
	return 0;
}

int audio_free(struct audio *audio)
{
	return 0;
}

int audio_set_state(struct audio *audio, enum audio_state state)
{
	return -ENOSYS;
}

int audio_get_state(struct audio *audio, enum audio_state *statep)
{
	return -ENOSYS;
}

int audio_set_volume(struct audio *audio, uint8_t volume)
{
	return -ENOSYS;
}

int audio_get_volume(struct audio *audio, uint8_t *volumep)
{
	return -ENOSYS;
}

int audio_set_speakers_enable(struct audio *audio, bool enable)
{
	return -ENOSYS;
}

int audio_get_speakers_enable(struct audio *audio, bool *enablep)
{
	return -ENOSYS;
}
