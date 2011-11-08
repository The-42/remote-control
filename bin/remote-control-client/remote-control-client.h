/*
 * Copyright (C) 2010-2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef CLI_H
#define CLI_H 1

#include "remote-control.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

#define gettext_noop(str) str

struct cli_command_info;
struct cli;

struct cli_command_info {
	const char *name;
	const char *summary;
	const char *help;
	struct option *options;
	int (*exec)(struct cli *cli, int argc, char *argv[]);
} __attribute__((aligned(COMMANDS_ALIGN)));

#define _command_ __attribute__((unused, section(".commands")))

extern const struct cli_command_info commands_array_start;
extern const struct cli_command_info commands_array_end;

#define command_foreach(pos) \
	for (pos = &commands_array_start; pos < &commands_array_end; pos++)

struct cli {
	const struct option *options;

	struct remote_client *client;
	const char *hostname;
	const char *service;

	unsigned int quiet;
	unsigned int verbose;

	char **argv;
	int argc;
};

enum {
	DUMP_PREFIX_NONE,
	DUMP_PREFIX_ADDRESS,
	DUMP_PREFIX_OFFSET,
};

int parse_bool(const char *string, bool *res);
enum remote_mixer_control parse_mixer_control(const char *control);
int mixer_input_source_name(enum remote_mixer_input_source source,
		char *buffer, size_t size);
enum remote_mixer_input_source parse_mixer_input_source(const char *source);
enum remote_audio_state parse_audio_state(const char *state);
int audio_state_name(enum remote_audio_state state, char* buffer, size_t size);

void print_hex_dump(FILE *fp, const char *prefix, int prefix_type,
		int rowsize, int groupsize, const void *buf, size_t len,
		bool ascii);

char **slice_strdup(char *argv[], int start, int end);
void strfreev(char *strv[]);
bool shell_str_needs_escape(const char *s);

#endif /* CLI_H */
