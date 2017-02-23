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

int backlight_create(struct backlight **backlightp)
{
	return 0;
}

int backlight_free(struct backlight *backlight)
{
	return 0;
}

int backlight_enable(struct backlight *backlight, bool enable)
{
	return -ENOSYS;
}

int backlight_is_enabled(struct backlight *backlight)
{
	return -ENOSYS;
}

int backlight_set(struct backlight *backlight, unsigned int brightness)
{
	return -ENOSYS;
}

int backlight_get(struct backlight *backlight)
{
	return -ENOSYS;
}
