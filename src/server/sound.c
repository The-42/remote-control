/*
 * Copyright (C) 2010-2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <glib.h>

#include "remote-control-stub.h"
#include "remote-control.h"

int32_t medcom_sound_play_async(void *priv, const char *filename)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, filename=%s)", __func__, priv, filename);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_sound_play_sync(void *priv, const char *filename)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, filename=%s)", __func__, priv, filename);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}
