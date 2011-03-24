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

#include <ctype.h>
#include <stdio.h>

#include "remote-control-stub.h"
#include "remote-control.h"

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

static gint g_hex_dump_to_buffer(gchar *buffer, size_t length, const void *data,
		size_t size, size_t rowsize, bool ascii)
{
	const guchar *ptr = data;
	const gchar *prefix = "";
	gint pos = 0;
	gint num;
	size_t i;

	for (i = 0; i < size; i++) {
		num = g_snprintf(buffer + pos, length - pos, "%s%02x", prefix,
				ptr[i]);
		prefix = " ";
		pos += num;
	}

	for (i = i; i < rowsize; i++) {
		num = g_snprintf(buffer + pos, length - pos, "   ");
		pos += num;
	}

	if (ascii) {
		num = g_snprintf(buffer + pos, length - pos, " |");
		pos += num;

		for (i = 0; i < size; i++) {
			if (isprint(ptr[i])) {
				num = g_snprintf(buffer + pos, length - pos,
						"%c", ptr[i]);
			} else {
				num = g_snprintf(buffer + pos, length - pos,
						".");
			}

			pos += num;
		}

		for (i = i; i < rowsize; i++) {
			num = g_snprintf(buffer + pos, length - pos, " ");
			pos += num;
		}

		num = g_snprintf(buffer + pos, length - pos, "|");
		pos += num;
	}

	return pos;
}

gboolean g_log_hex_dump(const gchar *domain, GLogLevelFlags flags,
		const gchar *prefix_str, int prefix_type, size_t rowsize,
		const void *buffer, size_t size, bool ascii)
{
	gsize length = (rowsize * 4) + 3;
	const guchar *ptr = buffer;
	gchar *line;
	size_t i;

	line = g_malloc(length);
	if (!line)
		return FALSE;

	for (i = 0; i < size; i += rowsize) {
		g_hex_dump_to_buffer(line, length, &ptr[i], min(size - i, rowsize), rowsize, ascii);

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
