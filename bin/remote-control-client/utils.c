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

#include <ctype.h>

#include "cli.h"

void shctl_log_hexdump(struct shctl *ctl, int level, const char *prefix,
		int prefix_type, int rowsize, int groupsize, const void *buf,
		size_t len, bool ascii)
{
	const uint8_t *ptr = buf;
	size_t i;
	size_t j;

	for (j = 0; j < len; j += rowsize) {
		switch (prefix_type) {
		case DUMP_PREFIX_ADDRESS:
			shctl_log(ctl, level, "%s%p: ", prefix, ptr + j);
			break;

		case DUMP_PREFIX_OFFSET:
			shctl_log(ctl, level, "%s%.8x: ", prefix, j);
			break;

		default:
			shctl_log(ctl, level, "%s", prefix);
			break;
		}

		for (i = 0; i < rowsize; i++) {
			if ((j + i) < len)
				shctl_log(ctl, level, "%s%02x", i ? " " : "", ptr[j + i]);
			else
				shctl_log(ctl, level, "%s  ", i ? " " : "");
		}

		shctl_log(ctl, level, " | ");

		for (i = 0; i < rowsize; i++) {
			if ((j + i) < len) {
				if (isprint(ptr[j + i]))
					shctl_log(ctl, level, "%c", ptr[j + i]);
				else
					shctl_log(ctl, level, ".");
			} else {
				shctl_log(ctl, level, " ");
			}
		}

		shctl_log(ctl, level, " |\n");
	}
}
