/*
 * Copyright (C) 2010 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef CLI_H
#define CLI_H 1

#include <libsh.h>

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

#endif /* CLI_H */
