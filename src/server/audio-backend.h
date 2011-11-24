/*
 * src/server/audio-backend.h
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

#ifndef AUDIO_BACKEND_H
#define AUDIO_BACKEND_H 1

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib-2.0/glib.h>

#include "remote-control-stub.h"
#include "remote-control.h"

struct audio_context;

/*
 * Defines the functions of a audio_context implementation.
 */
struct audio_impl {
	/* creation of implementation */
	int32_t (*create)(struct audio_context**);
	int32_t (*destroy)(struct audio_context*);
	/* functions provides by the audio_context implementation */
	int32_t (*set_state)(struct audio_context *,
	                     enum RPC_TYPE(audio_state), bool);
	int32_t (*get_state)(struct audio_context *,
	                     enum RPC_TYPE(audio_state) *);
	int32_t (*set_volume)(struct audio_context *, uint8_t);
	int32_t (*get_volume)(struct audio_context *, uint8_t *);
	int32_t (*enable_speakers)(struct audio_context *, bool);
	int32_t (*speakers_enabled)(struct audio_context *, bool *);
};

struct audio_impl* find_audio_impl();
struct audio_impl* audio_get_impl(struct audio*);
struct audio_context* audio_get_context(struct audio*);

#endif /* AUDIO_BACKEND_H */