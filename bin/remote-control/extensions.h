/*
 * Copyright (C) 2017 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef EXTENSIONS_H
#define EXTENSIONS_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include "remote-control.h"

/*
 * Extensions are used by dynamically loading shared libraries as needed. This
 * avoids build-time dependencies on extensions which in turn require the core
 * library. Symbols from extensions are defined as empty per default (mind the
 * parameter count when adding new ones). Depending on the configuration some or
 * all of them might be redefined to a real function further below.
 *
 * Using empty defines rather than empty bodies makes this header a bit more
 * complicated but it saves time and memory by avoiding parameter checks and
 * storing the symbol at all.
 */

#define extensions_init(x)
#define extensions_free(x)

#define ext_rpc_create(x, y)

#if ENABLE_EXTENSIONS

#undef extensions_init
void extensions_init(struct remote_control *rc);
#undef extensions_free
void extensions_free(struct remote_control *rc);

#if ENABLE_EXT_RCRPC
#undef ext_rpc_create
int ext_rpc_create(void *rcpriv, GKeyFile *config);
#endif /* ENABLE_EXT_RCRPC */

#endif /* ENABLE_EXTENSIONS */

#endif /* EXTENSIONS_H */
