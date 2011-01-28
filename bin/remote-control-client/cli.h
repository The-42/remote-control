/*
 * Copyright (C) 2010-2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef CLI_H
#define CLI_H 1

#include <libsh.h>

/*
 * TODO: get rid of the requirement to include this header file, because it
 *       will allow distributing the remote-control client library for use
 *       in external projects
 */
#include "remote-control-stub.h"
#include "remote-control.h"

#define gettext_noop(str) str

struct cli {
	const struct shcmd_def *commands;
	struct medcom_client *client;
	const char *hostname;
	const char *service;

	unsigned int imode;
	unsigned int quiet;
	unsigned int verbose;
};

extern const struct shcmd_def cli_commands[];

enum {
	DUMP_PREFIX_NONE,
	DUMP_PREFIX_ADDRESS,
	DUMP_PREFIX_OFFSET,
};

void shctl_log_hexdump(struct shctl *ctl, int level, const char *prefix,
		int prefix_type, int rowsize, int groupsize, const void *buf,
		size_t len, bool ascii);

#endif /* CLI_H */
