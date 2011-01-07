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

#include <getopt.h>
#include <libintl.h>
#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef USE_READLINE
#  include <readline/readline.h>
#  include <readline/history.h>
#endif

#include <libsh.h>

#include "cli.h"

static const char CLIENT_PROMPT[] = "medcom > ";
static const char DEFAULT_HOSTNAME[] = "localhost";
static const char DEFAULT_SERVICE[] = "7482";

static struct option opt[] = {
	{ "help", 0, 0, 'h' },
	{ "host", 1, 0, 'H' },
	{ "port", 1, 0, 'p' },
	{ "quiet", 0, 0, 'q' },
	{ "verbose", 0, 0, 'v' },
	{ "version", 0, 0, 'V' },
	{ NULL, 0, 0, 0 },
};

static const char help_options[] = "" \
	"  options:\n" \
	"    -h | --help               show this help screen\n" \
	"    -H | --host <hostname>    server host name\n" \
	"    -p | --port <port>        server port\n" \
	"    -q | --quiet              quiet mode\n" \
	"    -v | --verbose            increase verbosity level\n" \
	"    -V | --version            show version information\n";

#ifdef USE_READLINE
static const struct shcmd_def *shcmd_def_search(const char *name)
{
	const struct shcmd_def *cmd;

	for (cmd = cli_commands; cmd->name; cmd++) {
		if (strcmp(cmd->name, name) == 0)
			return cmd;
	}

	return NULL;
}

static char *readline_command_generator(const char *text, int state)
{
	static int list_index, len;
	const char *name;

	if (!state) {
		len = strlen(text);
		list_index = 0;
	}

	while ((name = cli_commands[list_index].name)) {
		list_index++;

		if (strncmp(name, text, len) == 0)
			return strdup(name);
	}

	return NULL;
}

static char *readline_options_generator(const char *text, int state)
{
	static const struct shcmd_def *cmd = NULL;
	static int list_index, len;
	const char *name;

	if (!state) {
		char *cmdname;
		char *p;

		if (!(p = strchr(rl_line_buffer, ' ')))
			return NULL;

		cmdname = malloc((p - rl_line_buffer) + 1);
		memcpy(cmdname, rl_line_buffer, p - rl_line_buffer);
		cmd = shcmd_def_search(cmdname);
		list_index = 0;
		len = strlen(text);
		free(cmdname);
	}

	if (!cmd || !cmd->opts)
		return NULL;

	while ((name = cmd->opts[list_index].name)) {
		const struct shcmd_opt_def *opt = &cmd->opts[list_index];
		char *ret;

		list_index++;

		if (opt->type == SHCMD_OT_DATA)
			continue;

		if (len > 2) {
			if (strncmp(name, text + 2, len - 2) != 0)
				continue;
		}

		ret = malloc(strlen(name) + 3);
		snprintf(ret, strlen(name) + 3, "--%s", name);
		return ret;
	}

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

static int cli_init(struct shctl *ctl)
{
	struct cli *cli = shctl_priv(ctl);
	struct rpc_host host;
	int err;

	memset(&host, 0, sizeof(host));
	host.hostname = cli->hostname;
	host.service = cli->service;

	err = medcom_init(&cli->client, cli->hostname, cli->service);
	if (err < 0) {
		fprintf(stderr, "medcom_init(): %s\n", strerror(-err));
		return err;
	}

	medcom_register_event_handler(cli->client, MEDCOM_EVENT_VOIP,
			on_voip_event, cli->client);

	return 0;
}

static int cli_exit(struct shctl *ctl)
{
	struct cli *cli = shctl_priv(ctl);

	medcom_exit(cli->client);

	return 0;
}

static int cli_parse(struct shctl *ctl, int argc, char *argv[])
{
	struct cli *cli = shctl_priv(ctl);
	int help = 0;
	int end = 0;
	int arg = 0;
	int idx = 0;

	end = shcmd_find_command(argc, argv, opt);
	if (end < 0) {
		fprintf(stderr, "shcmd_find_command(): %s\n", strerror(-end));
		return 1;
	}

	end = end ? end : argc;

	while ((arg = getopt_long(end, argv, "H:hp:qvV", opt, &idx)) != -1) {
		switch (arg) {
		case 'h':
			help = 1;
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
			fprintf(stdout, "%s\n", VERSION);
			break;

		default:
			fprintf(stderr, "unsupported option '-%c'. See "
					"--help.\n", arg);
			break;
		}
	}

	if (help) {
		if (end < argc) {
			fprintf(stderr, "extra argument '%s'. See --help\n",
					argv[end]);
			return 1;
		}

		shctl_usage(ctl, argv[0]);
		return 1;
	}

	if (argc > end) {
		char *command;
		int err;

		cli->imode = 0;

		command = shcmd_make_command(argv, end, argc);
		if (!command)
			return -ENOMEM;

		shctl_log(ctl, 2, "command: %s\n", command);

		err = shctl_parse_command(ctl, command);
		if (err < 0) {
			fprintf(stderr, "shctl_parse_command(): %s\n",
					strerror(-err));
			free(command);
			return err;
		}

		free(command);
	}

	return 0;
}

static int cli_usage(struct shctl *ctl, const char *program)
{
	struct cli *cli = shctl_priv(ctl);
	const struct shcmd_def *def;

	shctl_log(ctl, 0, "\n%s [options] [commands]\n\n", program);
	shctl_log(ctl, 0, help_options);
	shctl_log(ctl, 0, "\n  commands:\n");

	for (def = cli->commands; def->name; def++) {
		const char *help = shcmd_def_get_info(def, "help");
		shctl_log(ctl, 0, "    %-25s %s\n", def->name, help);
	}

	shctl_log(ctl, 0, "\n");
	shctl_log(ctl, 0, "  (specify help <command> for details about the command)\n");
	shctl_log(ctl, 0, "\n");

	return 0;
}

static int cli_log(struct shctl *ctl, int level, const char *fmt, va_list ap)
{
	struct cli *cli = shctl_priv(ctl);

	if (level > cli->verbose)
		return 0;

	return vprintf(fmt, ap);
}

static struct shctl_ops cli_ops = {
	.init = cli_init,
	.exit = cli_exit,
	.parse = cli_parse,
	.usage = cli_usage,
	.log = cli_log,
	.release = NULL,
};

int main(int argc, char *argv[])
{
	struct shctl *ctl;
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

	ctl = shctl_alloc(sizeof(*cli), &cli_ops, cli_commands);
	if (!ctl) {
		fprintf(stderr, "failed to allocated parser\n");
		return 1;
	}

	cli = shctl_priv(ctl);
	cli->commands = cli_commands;
	cli->client = NULL;
	cli->hostname = DEFAULT_HOSTNAME;
	cli->service = DEFAULT_SERVICE;
	cli->imode = 1;
	cli->quiet = 0;
	cli->verbose = 0;

	err = shctl_parse(ctl, argc, argv);
	if (err < 0) {
		fprintf(stderr, "shctl_parse(): %s\n", strerror(-err));
		shctl_free(ctl);
		return 1;
	}

	err = shctl_init(ctl);
	if (err < 0) {
		fprintf(stderr, "shctl_init(): %s\n", strerror(-err));
		shctl_free(ctl);
		return 1;
	}

	if (!cli->imode) {
		err = shctl_run(ctl, NULL);
	} else {
		const char *prompt = CLIENT_PROMPT;
		char *command = NULL;

		readline_init();

		do {
			command = readline_read(prompt);
			if (!command)
				break;

			if (*command) {
				readline_add_history(command);

				err = shctl_parse_command(ctl, command);
				if (err >= 0)
					err = shctl_run(ctl, NULL);
			}

			free(command);
			command = NULL;
		} while (cli->imode);

		if (!command)
			fputc('\n', stdout);

		readline_exit();
	}

	err = shctl_exit(ctl);
	if (err < 0) {
		fprintf(stderr, "shctl_exit(): %s\n", strerror(-err));
		return 1;
	}

	shctl_free(ctl);
	return 0;
}
