/*
 * Copyright (C) 2010-2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <ctype.h>
#include <stdio.h>

#include "remote-control-stub.h"
#include "remote-control.h"

void rc_logv(const char *fmt, va_list ap)
{
	vfprintf(stderr, fmt, ap);
}

void rc_log(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	rc_logv(fmt, ap);
	va_end(ap);
}

void print_hex_dump(const char *level, const char *prefix_str, int prefix_type,
		size_t rowsize, const void *buffer, size_t size, bool ascii)
{
	const uint8_t *ptr = buffer;
	const char *prefix = "";
	size_t i;
	size_t j;

	for (i = 0; i < size; i += rowsize) {
		switch (prefix_type) {
		case DUMP_PREFIX_ADDRESS:
			rc_log("%s%s%p: ", level, prefix_str, ptr + i);
			break;

		case DUMP_PREFIX_OFFSET:
			rc_log("%s%s%.8zx: ", level, prefix_str, i);
			break;

		default:
			rc_log("%s%s", level, prefix_str);
			break;
		}

		prefix = "";

		for (j = 0; j < rowsize; j++) {
			rc_log("%s%02x", prefix, ptr[i + j]);
			prefix = " ";
		}

		for (j = j; j < rowsize; j++)
			rc_log("   ");

		if (ascii) {
			rc_log(" |");

			for (j = 0; j < rowsize; j++) {
				if (isprint(ptr[i + j]))
					rc_log("%c", ptr[i + j]);
				else
					rc_log(".");
			}

			for (j = j; j < rowsize; j++)
				rc_log(" ");

			rc_log("|");
		}

		rc_log("\n");
	}
}
