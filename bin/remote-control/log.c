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

#include <stdarg.h>
#include <syslog.h>
#include <unistd.h>

#include <alsa/asoundlib.h>

#define pr_fmt(fmt) "log: " fmt

#include "glogging.h"
#include "log.h"

static void (*log_exit)(void *data) = NULL;
static void *log_data = NULL;

static void alsa_error_handler(const char *file, int line, const char *function,
			       int err, const char *fmt, ...)
{
	gchar *buffer;
	va_list ap;

	buffer = g_strdup_printf("alsa: %s:%d/%s(): %s", file, line, function,
				 fmt);

	va_start(ap, fmt);
	g_logv(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, buffer, ap);
	va_end(ap);

	g_free(buffer);
}

/*
 * stdio backend
 */

static void remote_control_stdio_log_handler(const gchar *log_domain,
		GLogLevelFlags log_level, const gchar *message,
		gpointer unused_data)
{
	void (*print)(const gchar *format, ...);
	const gchar *level_prefix = "LOG";
	const gchar *program;
	gchar buffer[16];
	struct tm *tmp;
	time_t now;

	switch (log_level & G_LOG_LEVEL_MASK) {
	case G_LOG_LEVEL_ERROR:
		level_prefix = "ERROR";
		print = g_printerr;
		break;

	case G_LOG_LEVEL_CRITICAL:
		level_prefix = "CRITICAL";
		print = g_printerr;
		break;

	case G_LOG_LEVEL_WARNING:
		level_prefix = "WARNING";
		print = g_printerr;
		break;

	case G_LOG_LEVEL_MESSAGE:
		level_prefix = "MESSAGE";
		print = g_printerr;
		break;

	case G_LOG_LEVEL_INFO:
		level_prefix = "INFO";
		print = g_print;
		break;

	case G_LOG_LEVEL_DEBUG:
		level_prefix = "DEBUG";
		print = g_print;
		break;

	default:
		print = g_print;
		break;
	}

	now = time(NULL);
	tmp = localtime(&now);
	strftime(buffer, sizeof(buffer), "%b %e %H:%M:%S", tmp);
	print("%s ", buffer);

	if (log_domain)
		print("%s - ", log_domain);
	else
		print("** ");

	program = g_get_prgname();
	if (program)
		print("(%s:%lu): ", program, getpid());
	else
		print("(process:%lu): ", getpid());

	print("%s: %s\n", level_prefix, message ?: "(NULL) message");
}

/*
 * syslog backend
 */

static void remote_control_syslog_log_handler(const gchar *log_domain,
		GLogLevelFlags log_level, const gchar *message, gpointer data)
{
	int priority;

	/*
	 * Note that GLib actually has the error and critical levels in
	 * opposite priority. Critical messages are not as important as
	 * error messages. Error messages are typically fatal.
	 */
	switch (log_level & G_LOG_LEVEL_MASK) {
	case G_LOG_LEVEL_ERROR:
		priority = LOG_CRIT;
		break;

	case G_LOG_LEVEL_CRITICAL:
		priority = LOG_ERR;
		break;

	case G_LOG_LEVEL_WARNING:
		priority = LOG_WARNING;
		break;

	case G_LOG_LEVEL_MESSAGE:
		priority = LOG_NOTICE;
		break;

	case G_LOG_LEVEL_INFO:
		priority = LOG_INFO;
		break;

	case G_LOG_LEVEL_DEBUG:
	default:
		priority = LOG_DEBUG;
		break;
	}

	if (log_domain)
		syslog(priority, "%s - %s", log_domain, message);
	else
		syslog(priority, "%s", message);
}

static void remote_control_syslog_exit(void *data)
{
	closelog();
}

/*
 * public interface
 */

void remote_control_log_early_init(void)
{
	g_log_set_default_handler(remote_control_stdio_log_handler, NULL);
	snd_lib_error_set_handler(alsa_error_handler);
}

int remote_control_log_init(GKeyFile *conf)
{
	GLogFunc handler = remote_control_stdio_log_handler;
	const gchar *backend = "stdio";
	gpointer data = NULL;
	gchar *target;

	target = g_key_file_get_value(conf, "logging", "target", NULL);
	if (target) {
		if (g_str_equal(target, "syslog")) {
			const gchar *program = g_get_prgname();

			openlog(program, LOG_CONS | LOG_PID, LOG_LOCAL0);

			handler = remote_control_syslog_log_handler;
			log_exit = remote_control_syslog_exit;
			backend = "syslog";
		}

		g_free(target);
	}

	pr_debug("switching to %s backend", backend);
	g_log_set_default_handler(handler, data);

	return 0;
}

void remote_control_log_exit(void)
{
	if (log_exit)
		log_exit(log_data);
}
