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

#include <errno.h>

#include "remote-control-stub.h"
#include "remote-control.h"

struct media_player {
};


int media_player_create(struct media_player **playerp)
{
	return -ENOSYS;
}

int media_player_free(struct media_player *player)
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

int media_player_get_state(struct media_player *player,
		enum media_player_state *statep)
{
	return -ENOSYS;
}
