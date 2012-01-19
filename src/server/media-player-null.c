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

#include <errno.h>

#include "remote-control-stub.h"
#include "remote-control.h"

struct media_player {
	enum media_player_state state;
};

int media_player_create(struct media_player **playerp)
{
	struct media_player *player;

	if (!playerp)
		return -EINVAL;

	player = malloc(sizeof(*player));
	if (!player)
		return -ENOMEM;

	memset(player, 0, sizeof(*player));
	player->state = MEDIA_PLAYER_STOPPED;

	*playerp = player;
	return 0;
}

int media_player_free(struct media_player *player)
{
	if (!player)
		return -EINVAL;

	free(player);
	return 0;
}

int media_player_set_output_window(struct media_player *player,
		unsigned int x, unsigned int y, unsigned int width,
		unsigned int height)
{
	if (!player)
		return -EINVAL;

	return -ENOSYS;
}

int media_player_set_uri(struct media_player *player, const char *uri)
{
	if (!player || !uri)
		return -EINVAL;

	return -ENOSYS;
}

int media_player_get_uri(struct media_player *player, char **urip)
{
	if (!player || !urip)
		return -EINVAL;

	return -ENOSYS;
}

int media_player_play(struct media_player *player)
{
	if (!player)
		return -EINVAL;

	return -ENOSYS;
}

int media_player_stop(struct media_player *player)
{
	if (!player)
		return -EINVAL;

	return -ENOSYS;
}

int media_player_pause(struct media_player *player)
{
	g_return_val_if_fail(player != NULL, -EINVAL);

	return -ENOSYS;
}

int media_player_resume(struct media_player *player)
{
	g_return_val_if_fail(player != NULL, -EINVAL);

	return -ENOSYS;
}

int media_player_get_duration(struct media_player *player,
		unsigned long *duration)
{
	g_return_val_if_fail(player != NULL, -EINVAL);

	return -ENOSYS;
}

int media_player_get_position(struct media_player *player,
		unsigned long *position)
{
	g_return_val_if_fail(player != NULL, -EINVAL);

	return -ENOSYS;
}

int media_player_set_position(struct media_player *player,
		unsigned long position)
{
	g_return_val_if_fail(player != NULL, -EINVAL);

	return -ENOSYS;
}

int media_player_get_state(struct media_player *player,
		enum media_player_state *statep)
{
	if (!player || !statep)
		return -EINVAL;

	*statep = player->state;
	return 0;
}
