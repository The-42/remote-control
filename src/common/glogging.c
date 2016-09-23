/*
 * Copyright (C) 2012 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "glogging.h"

static gssize g_hex_dump_to_buffer(gchar *buffer, gsize length,
				   gconstpointer data, gsize size,
				   gsize rowsize, gboolean ascii)
{
	const guchar *ptr = data;
	const gchar *prefix = "";
	gint pos = 0, num;
	gsize i;

	for (i = 0; i < size; i++) {
		num = g_snprintf(buffer + pos, length - pos, "%s%02x", prefix,
				ptr[i]);
		prefix = " ";
		pos += num;
	}

	while (i++ < rowsize) {
		num = g_snprintf(buffer + pos, length - pos, "   ");
		pos += num;
	}

	if (ascii) {
		num = g_snprintf(buffer + pos, length - pos, " |");
		pos += num;

		for (i = 0; i < size; i++) {
			if (g_ascii_isprint(ptr[i])) {
				num = g_snprintf(buffer + pos, length - pos,
						"%c", ptr[i]);
			} else {
				num = g_snprintf(buffer + pos, length - pos,
						".");
			}

			pos += num;
		}

		while (i++ < rowsize) {
			num = g_snprintf(buffer + pos, length - pos, " ");
			pos += num;
		}

		num = g_snprintf(buffer + pos, length - pos, "|");
		pos += num;
	}

	return pos;
}

gboolean g_log_hex_dump(const gchar *domain, GLogLevelFlags flags,
			const gchar *prefix_str, gint prefix_type,
			gsize rowsize, gconstpointer buffer, gsize size,
			gboolean ascii)
{
	gsize length = (rowsize * 4) + 3, i;
	const guchar *ptr = buffer;
	gchar *line;

	line = g_malloc(length);
	if (!line)
		return FALSE;

	for (i = 0; i < size; i += rowsize) {
		g_hex_dump_to_buffer(line, length, &ptr[i], MIN(size - i, rowsize), rowsize, ascii);

		switch (prefix_type) {
		case DUMP_PREFIX_ADDRESS:
			g_log(domain, flags, "%s%p: %s", prefix_str, ptr + i, line);
			break;

		case DUMP_PREFIX_OFFSET:
			g_log(domain, flags, "%s%.8zx: %s", prefix_str, i, line);
			break;

		default:
			g_log(domain, flags, "%s%s", prefix_str, line);
			break;
		}
	}

	g_free(line);
	return TRUE;
}
