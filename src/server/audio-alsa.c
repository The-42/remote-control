/*
 * src/server/audio-alsa.c (ALSA UCM BACKEND)
 *
 * Authors: Soeren Grunewald <soeren.grunewald@avionic-design.de>
 *
 * Copyright (C) 2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "audio-backend.h"

#include <errno.h>


/*
 * Here comes the public stuff...
 */

struct audio_impl *find_audio_impl()
{
	return NULL;
}

struct audio {
	struct audio_impl *impl;
	struct audio_context *ctx;
};

struct audio_context* audio_get_context(struct audio *audio)
{
	return audio ? audio->ctx : NULL;
}

struct audio_impl* audio_get_impl(struct audio *audio)
{
	return audio ? audio->impl : NULL;
}

int audio_create(struct audio **audiop)
{
	struct audio *audio;
	int ret = 0;

	audio = g_new0(struct audio, 1);
	if (!audio)
		return -ENOMEM;

	audio->impl = find_audio_impl();
	if (audio->impl && audio->impl->create) {
		ret = audio->impl->create(&audio->ctx);
		if (ret < 0)
			g_warning("%s: failed to create context", __func__);
	}
	*audiop = audio;

	return ret;
}

int audio_free(struct audio *audio)
{
	int err = 0;

	if (!audio)
		return -EINVAL;

	audio->impl = find_audio_impl();
	if (audio->impl && audio->impl->destroy) {
		err = audio->impl->destroy(audio->ctx);
		if (err < 0)
			g_warning("%s: failed to destroy: %d", __func__, err);
	}
	g_free(audio);
	audio = NULL;

	return err;
}