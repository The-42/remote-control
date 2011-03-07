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

#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <libintl.h>
#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef USE_READLINE
#  include <readline/readline.h>
#  include <readline/history.h>
#endif

#include <librpc.h>

#include "remote-control-client.h"

static const char CLIENT_PROMPT[] = "remote > ";
static const char DEFAULT_HOSTNAME[] = "localhost";
static const char DEFAULT_SERVICE[] = "7482";

static struct option options[] = {
	{ "help", 0, 0, 'h' },
	{ "host", 1, 0, 'H' },
	{ "port", 1, 0, 'p' },
	{ "quiet", 0, 0, 'q' },
	{ "verbose", 0, 0, 'v' },
	{ "version", 0, 0, 'V' },
	{ NULL, 0, 0, 0 },
};

static const char help_options[] = "" \
	"  Options:\n" \
	"    -h, --help                show this help screen\n" \
	"    -H, --host <hostname>     server host name\n" \
	"    -p, --port <port>         server port\n" \
	"    -q, --quiet               quiet mode\n" \
	"    -v, --verbose             increase verbosity level\n" \
	"    -V, --version             show version information\n";

#ifdef USE_READLINE
static const struct cli_command_info *find_command_by_name(const char *name)
{
	const struct cli_command_info *cmd;

	command_foreach (cmd) {
		if (strcmp(cmd->name, name) == 0)
			return cmd;
	}

	return NULL;
}

static char *readline_command_generator(const char *text, int state)
{
	static const struct cli_command_info *cmd;
	static int len;

	if (state == 0) {
		cmd = &commands_array_start;
		len = strlen(text);
	}

	while (cmd < &commands_array_end) {
		if (strncmp(cmd->name, text, len) == 0)
			return strdup((cmd++)->name);

		cmd++;
	}

	return NULL;
}

static char *readline_options_generator(const char *text, int state)
{
	static const struct cli_command_info *cmd = NULL;
	static int list_index, len;

	if (!state) {
		char *cmdname;
		char *p;

		if (!(p = strchr(rl_line_buffer, ' ')))
			return NULL;

		cmdname = malloc((p - rl_line_buffer) + 1);
		memcpy(cmdname, rl_line_buffer, p - rl_line_buffer);
		cmd = find_command_by_name(cmdname);
		list_index = 0;
		len = strlen(text);
		free(cmdname);
	}

	if (!cmd || !cmd->options)
		return NULL;

	/* TODO: look up option arguments */

	return NULL;
}

static char **readline_completion(const char *text, int start, int end)
{
	char **matches = NULL;

	if (start == 0)
		matches = rl_completion_matches(text, readline_command_generator);
	else
		matches = rl_completion_matches(text, readline_options_generator);

	return matches;
}

static void readline_init(void)
{
	rl_readline_name = "medcom-client";
	rl_attempted_completion_function = readline_completion;
	stifle_history(500);
}

static char *readline_read(const char *prompt)
{
	return readline(prompt);
}

static void readline_add_history(const char *string)
{
	add_history(string);
}

static void readline_exit(void)
{
}
#else
static void readline_init(void)
{
}

static char *readline_read(const char *prompt)
{
	char line[1024];
	char *r;
	int len;

	fputs(prompt, stdout);

	r = fgets(line, sizeof(line), stdin);
	if (!r)
		return NULL;

	len = strlen(r);

	if ((len > 0) && (r[len - 1] == '\n'))
		r[len - 1] = '\0';

	return strdup(r);
}

static void readline_add_history(const char *string)
{
}

static void readline_exit(void)
{
}
#endif

static void on_voip_event(uint32_t type, void *data)
{
	printf("> %s(type=%08x, data=%p)\n", __func__, type, data);
	printf("< %s()\n", __func__);
}

static const char *skip_spaces(const char *ptr)
{
	while (isspace(*ptr))
		ptr++;

	return ptr;
}

static char *unquote(const char *s, size_t n)
{
	size_t length = 0;
	char quote = 0;
	char *result;
	size_t i;

	for (i = 0; i < n; i++) {
		if ((s[i] == '"') || (s[i] == '\'')) {
			if (quote != 0) {
				if (quote == s[i])
					quote = 0;
				else
					length++;
			} else {
				quote = s[i];
			}
		} else {
			length++;
		}
	}

	result = malloc(length + 1);
	if (!result)
		return NULL;

	for (i = 0, length = 0; i < n; i++) {
		if ((s[i] == '"') || (s[i] == '\'')) {
			if (quote != 0) {
				if (quote == s[i])
					quote = 0;
				else
					result[length++] = s[i];
			} else {
				quote = s[i];
			}
		} else {
			result[length++] = s[i];
		}
	}

	result[length] = '\0';
	return result;
}

static int shell_parse(const char *string, int argc, char *argv[])
{
	const char *last;
	const char *ptr;
	char quote = 0;
	int count = 0;

	last = ptr = skip_spaces(string);

	while (*ptr) {
		if ((*ptr == '"') || (*ptr == '\'')) {
			if (quote != 0) {
				if (quote == *ptr)
					quote = 0;
			} else {
				quote = *ptr;
			}

			ptr++;
		} else {
			if ((quote == 0) && isspace(*ptr)) {
				size_t length = ptr - last;

				if (count < argc)
					argv[count] = unquote(last, length);

				last = ptr = skip_spaces(ptr);
				count++;
			} else {
				ptr++;
			}
		}
	}

	if ((ptr - last) > 0) {
		if (count < argc)
			argv[count] = unquote(last, ptr - last);

		count++;
	}

	return count;
}

static int cli_parse_command(struct cli *cli, const char *command)
{
	int ret;

	strfreev(cli->argv);
	cli->argv = NULL;
	cli->argc = 0;

	ret = shell_parse(command, 0, NULL);
	if (ret < 0)
		return ret;

	cli->argv = calloc(ret + 1, sizeof(char *));
	if (!cli->argv)
		return -ENOMEM;

	cli->argc = ret;

	ret = shell_parse(command, cli->argc, cli->argv);
	if (ret < 0) {
		strfreev(cli->argv);
		cli->argv = NULL;
		cli->argc = 0;
	}

	return ret;
}

static int cli_exec(struct cli *cli)
{
	const struct cli_command_info *cmd;

	command_foreach (cmd) {
		if (strcmp(cmd->name, cli->argv[0]) == 0)
			return cmd->exec(cli, cli->argc, cli->argv);
	}

	printf("%s: command not found\n", cli->argv[0]);
	return -ENOSYS;
}

static struct cli *cli_new(const struct option *options)
{
	struct cli *cli;

	cli = calloc(1, sizeof(*cli));
	if (!cli)
		return NULL;

	cli->options = options;
	cli->client = NULL;
	cli->hostname = DEFAULT_HOSTNAME;
	cli->service = DEFAULT_SERVICE;
	cli->argv = NULL;
	cli->argc = 0;
	cli->quiet = 0;
	cli->verbose = 0;

	return cli;
}

static int cli_free(struct cli *cli)
{
	if (!cli)
		return -EINVAL;

	remote_exit(cli->client);
	strfreev(cli->argv);
	free(cli);
	return 0;
}

static void cli_usage(struct cli *cli, FILE *fp, const char *program)
{
	const struct cli_command_info *cmd;
	char *copy = strdup(program);

	fprintf(fp, "\nUsage: %s [options] [commands]\n\n", basename(copy));
	fprintf(fp, help_options);
	fprintf(fp, "\n  Commands:\n");

	command_foreach (cmd)
		fprintf(fp, "    %-25s %s\n", cmd->name, cmd->summary);

	fprintf(fp, "\n");
	fprintf(fp, "  (specify help <command> for details about the command)\n");
	fprintf(fp, "\n");

	free(copy);
}

static int cli_init(struct cli *cli)
{
	struct rpc_host host;
	int err;

	memset(&host, 0, sizeof(host));
	host.hostname = cli->hostname;
	host.service = cli->service;

	err = remote_init(&cli->client, cli->hostname, cli->service);
	if (err < 0) {
		fprintf(stderr, "remote_init(): %s\n", strerror(-err));
		return err;
	}

	remote_register_event_handler(cli->client, REMOTE_EVENT_VOIP,
			on_voip_event, cli->client);

	return 0;
}

static int cli_parse_command_line(struct cli *cli, int argc, char *argv[])
{
	bool version = false;
	bool help = false;
	int opt = 0;

	while ((opt = getopt_long(argc, argv, "H:hp:qvV", cli->options, NULL)) != -1) {
		switch (opt) {
		case 'h':
			help = true;
			break;

		case 'H':
			cli->hostname = optarg;
			break;

		case 'p':
			cli->service = optarg;
			break;

		case 'q':
			cli->quiet = 1;
			break;

		case 'v':
			cli->verbose++;
			break;

		case 'V':
			version = true;
			break;

		default:
			fprintf(stderr, "unsupported option '-%c'. See "
					"--help.\n", opt);
			break;
		}
	}

	if (help) {
		cli_usage(cli, stdout, argv[0]);
		return 1;
	}

	if (version) {
		char *copy = strdup(argv[0]);
		printf("%s %s\n", basename(copy), VERSION);
		free(copy);
		return 1;
	}

	if (argc > optind) {
		cli->argv = slice_strdup(argv, optind, argc);
		cli->argc = argc - optind;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	struct cli *cli;
	int err;

	if (!setlocale(LC_ALL, ""))
		perror("setlocale");

	if (!bindtextdomain(GETTEXT_PACKAGE, LOCALEBASEDIR)) {
		perror("bindtextdomain");
		return 1;
	}

	if (!textdomain(GETTEXT_PACKAGE)) {
		perror("textdomain");
		return 1;
	}

	cli = cli_new(options);
	if (!cli) {
		fprintf(stderr, "failed to allocate application context\n");
		return 1;
	}

	err = cli_parse_command_line(cli, argc, argv);
	if (err != 0) {
		if (err < 0) {
			cli_usage(cli, stderr, argv[0]);
			cli_free(cli);
			return 1;
		}

		return 0;
	}

	err = cli_init(cli);
	if (err < 0) {
		fprintf(stderr, "shctl_init(): %s\n", strerror(-err));
		cli_free(cli);
		return 1;
	}

	if (!cli->argc) {
		const char *prompt = CLIENT_PROMPT;
		char *command = NULL;

		readline_init();

		while (true) {
			command = readline_read(prompt);
			if (!command)
				break;

			if (*command) {
				readline_add_history(command);

				err = cli_parse_command(cli, command);
				if (err >= 0)
					cli_exec(cli);
			}

			free(command);
			command = NULL;
		}

		if (!command)
			fputc('\n', stdout);

		readline_exit();
	} else {
		cli_exec(cli);
	}

	cli_free(cli);
	return 0;
}
