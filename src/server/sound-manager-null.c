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

#include "remote-control-stub.h"
#include "remote-control.h"

struct sound_manager {
	int foralloc;
};

int sound_manager_create(struct sound_manager **managerp)
{
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

out:
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}
