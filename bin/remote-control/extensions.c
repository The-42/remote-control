/*
 * Copyright (C) 2017 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dlfcn.h>
#include <glib.h>

#include "extensions.h"

#define LIBRCRPC_SO	"librcrpc.so.0.3.0"

struct so_rpc_data {
	void *handle;
	int (*rpc_create)(void *, GKeyFile *);
};

#if ENABLE_EXT_RCRPC
static struct so_rpc_data so_rpc;

static void ext_rcrpc_init(struct remote_control *rc)
{
	char *err;

	so_rpc.handle = dlopen(LIBRCRPC_SO, RTLD_LAZY);
	if (!so_rpc.handle) {
		g_warning("Failed to load %s: %s", LIBRCRPC_SO, dlerror());
		g_warning("%s: RPC extension NOT loaded.", __func__);
		return;
	}
	dlerror();

	*(void **) (&so_rpc.rpc_create) = dlsym(so_rpc.handle, "rcrpc_create");
	if ((err = dlerror()) != NULL)  {
		g_warning("%s: 'rcrpc_create': %s\n", LIBRCRPC_SO, err);
		so_rpc.rpc_create = NULL;
	}

	g_debug("%s: RPC extension loaded (%p)", __func__, rc);
}

static void ext_rcrpc_free(struct remote_control *rc)
{
	if (so_rpc.handle)
		dlclose(so_rpc.handle);

	so_rpc.rpc_create = NULL;

	g_debug("%s: RPC extension unloaded (%p)", __func__, rc);
}

int ext_rpc_create(void *rcpriv, GKeyFile *config)
{
	int err;

	if (!so_rpc.rpc_create) {
		g_warning("%s: missing extension symbol rpc_create", __func__);
		return -ENOSYS;
	}

	err = (*so_rpc.rpc_create)(rcpriv, config);
	if (err < 0) {
		g_warning("rpc_create(): %s", strerror(-err));
		return err;
	}

	return 0;
}

#else
#define ext_rcrpc_init(x)
#define ext_rcrpc_free(x)
#endif

void extensions_init(struct remote_control *rc)
{
	ext_rcrpc_init(rc);
}

void extensions_free(struct remote_control *rc)
{
	ext_rcrpc_free(rc);
}

