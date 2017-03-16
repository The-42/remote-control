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

#include "remote-control.h"

int handset_create(struct handset **handsetp, struct remote_control *rc)
{
	return 0;
}

int handset_free(struct handset *handset)
{
	return 0;
}

int handset_display_clear(struct handset *handset)
{
	return 0;
}

int handset_display_sync(struct handset *handset)
{
	return 0;
}

int handset_display_set_brightness(struct handset *handset,
		unsigned int brightness)
{
	return 0;
}

int handset_keypad_set_brightness(struct handset *handset,
		unsigned int brightness)
{
	return 0;
}

int handset_icon_show(struct handset *handset, unsigned int id, bool show)
{
	return 0;
}

int handset_text_show(struct handset *handset, unsigned int x, unsigned int y,
		const char *text, bool show)
{
	return 0;
}
