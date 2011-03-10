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

#include "remote-control-stub.h"
#include "remote-control.h"

#define TASK_MANAGER_PID_MIN 2

struct task {
	GPid real_pid;
	int32_t pid;
};

struct task_manager {
	int32_t last_pid;
	GList *tasks;
};

static void task_free(gpointer data)
{
	struct task *task = data;

	g_spawn_close_pid(task->real_pid);
	g_free(task);
}

static void child_watch(GPid pid, gint status, gpointer data)
{
	struct task_manager *manager = data;
	GList *tasks = manager->tasks;
	GList *node;

	for (node = g_list_first(tasks); node; node = g_list_next(node)) {
		struct task *task = node->data;

		if (task->real_pid == pid) {
			g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO, "child %d/%d "
					"exited with status %d", task->pid,
					task->real_pid, status);
			manager->tasks = g_list_delete_link(tasks, node);
			task_free(task);
			break;
		}
	}
}

static void check_pid(gpointer data, gpointer user_data)
{
	struct task *task = data;
	int32_t *pid = user_data;

	if (task->pid == *pid)
		(*pid)++;
}

static int32_t task_manager_get_next_pid(struct task_manager *manager)
{
	int32_t pid = manager->last_pid + 1;

	while (TRUE) {
		if (pid < TASK_MANAGER_PID_MIN)
			pid = TASK_MANAGER_PID_MIN;

		g_list_foreach(manager->tasks, check_pid, &pid);

		if (pid == (manager->last_pid + 1))
			break;
	}

	return pid;
}

int task_manager_create(struct task_manager **managerp)
{
	struct task_manager *manager;

	manager = g_new0(struct task_manager, 1);
	if (!manager)
		return -ENOMEM;

	manager->last_pid = TASK_MANAGER_PID_MIN - 1;

	*managerp = manager;
	return 0;
}

int task_manager_free(struct task_manager *manager)
{
	if (!manager)
		return -EINVAL;

	g_list_free_full(manager->tasks, task_free);
	g_free(manager);
	return 0;
}

int32_t task_manager_exec(void *priv, const char *command_line)
{
	struct task_manager *manager = remote_control_get_task_manager(priv);
	const gchar *display;
	GError *error = NULL;
	gchar **argv = NULL;
	gchar **envp = NULL;
	struct task *task;
	int32_t ret = 0;
	gint argc = 0;

	task = g_new0(struct task, 1);
	if (!task)
		return -ENOMEM;

	task->pid = task_manager_get_next_pid(manager);

	display = g_getenv("DISPLAY");
	if (display) {
		envp = g_new0(char *, 2);
		if (!envp) {
			ret = -ENOMEM;
			goto free;
		}

		envp[0] = g_strdup_printf("DISPLAY=%s", display);
		envp[1] = NULL;
	}

	if (!g_shell_parse_argv(command_line, &argc, &argv, &error)) {
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_ERROR, "failed to parse "
				"command-line: %s", error->message);
		g_error_free(error);
		ret = -EACCES;
		goto free;
	}

	if (!g_spawn_async(NULL, argv, envp, G_SPAWN_DO_NOT_REAP_CHILD, NULL,
				NULL, &task->real_pid, &error)) {
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_ERROR, "failed to execute "
				"child process: %s", error->message);
		g_error_free(error);
		ret = -EACCES;
		goto free;
	}

	g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO, "running \"%s\" (PID: %d/%d)",
			command_line, task->pid, task->real_pid);
	g_child_watch_add(task->real_pid, child_watch, manager);
	manager->tasks = g_list_append(manager->tasks, task);
	manager->last_pid = task->pid;

	ret = task->pid;
	goto out;

free:
	g_free(task);
out:
	g_strfreev(envp);
	g_strfreev(argv);
	return ret;
}

int32_t task_manager_kill(void *priv, int32_t pid, int32_t sig)
{
	struct task_manager *manager = remote_control_get_task_manager(priv);
	GList *tasks = manager->tasks;
	int32_t ret = -ESRCH;
	GList *node;

	for (node = g_list_first(tasks); node; node = g_list_next(node)) {
		struct task *task = node->data;

		if (task->pid == pid) {
			manager->tasks = g_list_delete_link(tasks, node);
			ret = kill(task->real_pid, sig);
			g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO, "child %d/%d "
					"killed with signal %d (%d)",
					task->pid, task->real_pid, sig, ret);
			task_free(task);
			break;
		}
	}

	return ret;
}
