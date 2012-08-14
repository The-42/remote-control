/*
 * Copyright (C) 2012 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __G_LOGGING_H__
#define __G_LOGGING_H__

#include <glib.h>

G_BEGIN_DECLS

enum {
	DUMP_PREFIX_NONE,
	DUMP_PREFIX_ADDRESS,
	DUMP_PREFIX_OFFSET,
};

gboolean g_log_hex_dump(const gchar *domain, GLogLevelFlags flags,
			const gchar *prefix_str, gint prefix_type,
			gsize rowsize, gconstpointer buffer, gsize size,
			gboolean ascii);

#ifndef pr_fmt
#define pr_fmt(fmt) fmt
#endif

#define pr_debug(fmt, args...) g_debug(pr_fmt(fmt), ##args)

G_END_DECLS

#endif
