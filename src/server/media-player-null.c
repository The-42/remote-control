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

int media_player_create(struct media_player **playerp, GKeyFile *config)
{
	return 0;
}

int media_player_free(struct media_player *player)
{
	return 0;
}

int media_player_set_crop(struct media_player *player,
		unsigned int left, unsigned int right, unsigned int top,
		unsigned int bottom)
{
	return -ENOSYS;
}

int media_player_set_output_window(struct media_player *player,
		unsigned int x, unsigned int y, unsigned int width,
		unsigned int height)
{
	return -ENOSYS;
}

int media_player_set_uri(struct media_player *player, const char *uri)
{
	return -ENOSYS;
}

int media_player_get_uri(struct media_player *player, char **urip)
{
	return -ENOSYS;
}

int media_player_play(struct media_player *player)
{
	return -ENOSYS;
}

int media_player_stop(struct media_player *player)
{
	return -ENOSYS;
}

int media_player_pause(struct media_player *player)
{
	return -ENOSYS;
}

int media_player_resume(struct media_player *player)
{
	return -ENOSYS;
}

int media_player_get_duration(struct media_player *player,
		unsigned long *duration)
{
	return -ENOSYS;
}

int media_player_get_position(struct media_player *player,
		unsigned long *position)
{
	return -ENOSYS;
}

int media_player_set_position(struct media_player *player,
		unsigned long position)
{
	return -ENOSYS;
}

int media_player_get_state(struct media_player *player,
		enum media_player_state *statep)
{
	return -ENOSYS;
}

int media_player_get_mute(struct media_player *player, bool *mute)
{
	return -ENOSYS;
}

int media_player_set_mute(struct media_player *player, bool mute)
{
	return -ENOSYS;
}

int media_player_get_audio_track_count(struct media_player *player, int *count)
{
	return -ENOSYS;
}

int media_player_get_audio_track_pid(struct media_player *player, int pos, int *pid)
{
	return -ENOSYS;
}

int media_player_get_audio_track_name(struct media_player *player, int pid, char **name)
{
	return -ENOSYS;
}

int media_player_get_audio_track(struct media_player *player, int *pid)
{
	return -ENOSYS;
}

int media_player_set_audio_track(struct media_player *player, int pid)
{
	return -ENOSYS;
}

int media_player_get_spu_count(struct media_player *player, int *count)
{
	return -ENOSYS;
}

int media_player_get_spu_pid(struct media_player *player, int pos, int *pid)
{
	return -ENOSYS;
}

int media_player_get_spu_name(struct media_player *player, int pid, char **name)
{
	return -ENOSYS;
}

int media_player_get_spu(struct media_player *player, int *pid)
{
	return -ENOSYS;
}

int media_player_set_spu(struct media_player *player, int pid)
{
	return -ENOSYS;
}

int media_player_get_teletext(struct media_player *player, int *page)
{
	return -ENOSYS;
}

int media_player_set_teletext(struct media_player *player, int page)
{
	return -ENOSYS;
}

int media_player_toggle_teletext_transparent(struct media_player *player)
{
	return -ENOSYS;
}

int media_player_set_es_changed_callback(struct media_player *player,
		media_player_es_changed_cb callback, void *data,
		void *owner_ref)
{
	return -ENOSYS;
}

void *media_player_get_es_changed_callback_owner(struct media_player *player)
{
	return NULL;
}
