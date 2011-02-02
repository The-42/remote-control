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

#include <vlc/vlc.h>

#include "remote-control-stub.h"
#include "remote-control.h"

#define LIBVLC_AUDIO_VOLUME_MAX 200

struct sound_manager {
	libvlc_instance_t *vlc;
	libvlc_media_player_t *player;
	libvlc_event_manager_t *evman;
	libvlc_media_t *media;
};

int sound_manager_create(struct sound_manager **managerp)
{
	const char *const argv[] = { "--file-caching", "0", NULL };
	int argc = 2;
	struct sound_manager *manager;
	int ret = 0;

	g_debug("> %s(managerp=%p)", __func__, managerp);

	if (!managerp) {
		ret = -EINVAL;
		goto out;
	}

	manager = g_malloc0(sizeof(*manager));
	if (!manager) {
		ret = -ENOMEM;
		goto out;
	}

	manager->vlc = libvlc_new(argc, argv);
	manager->player = libvlc_media_player_new(manager->vlc);
	libvlc_audio_set_volume(manager->player, LIBVLC_AUDIO_VOLUME_MAX);

	*managerp = manager;

out:
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int sound_manager_free(struct sound_manager *manager)
{
	int ret = 0;

	g_debug("> %s(manager=%p)", __func__, manager);

	if (!manager) {
		ret = -EINVAL;
		goto out;
	}

	libvlc_media_player_release(manager->player);
	libvlc_media_release(manager->media);
	libvlc_release(manager->vlc);
	g_free(manager);

out:
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int sound_manager_play(struct sound_manager *manager, const char *uri)
{
	int ret = 0;

	g_debug("> %s(manager=%p, uri=%s)", __func__, manager, uri);

	if (!manager || !uri) {
		ret = -EINVAL;
		goto out;
	}

	if (manager->media) {
		libvlc_media_release(manager->media);
		manager->media = NULL;
	}

	manager->media = libvlc_media_new_location(manager->vlc, uri);
	libvlc_media_player_set_media(manager->player, manager->media);
	libvlc_media_player_play(manager->player);

out:
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}
