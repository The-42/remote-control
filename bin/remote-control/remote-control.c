/*
 * Copyright (C) 2010-2012 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>
#include <glib.h>
#include <glib-unix.h>
#include <gio/gio.h>

#include <gudev/gudev.h>

#include "remote-control-webkit-window.h"
#include "remote-control-rdp-window.h"
#include "remote-control.h"
#include "gdevicetree.h"
#include "javascript.h"
#include "gkeyfile.h"
#include "glogging.h"
#include "utils.h"
#include "log.h"

#define RDP_DELAY_MIN  90
#define RDP_DELAY_MAX 120

#ifdef ENABLE_DBUS
static const gchar REMOTE_CONTROL_BUS_NAME[] = "de.avionic-design.RemoteControl";

static void g_dbus_remote_control_method_call(GDBusConnection *connection,
		const gchar *sender, const gchar *object,
		const gchar *interface, const gchar *method,
		GVariant *parameters, GDBusMethodInvocation *invocation,
		gpointer user_data)
{
	g_debug("> %s(connection=%p, sender=%s, object=%s, interface=%s, "
			"method=%s, parameters=%p, invocation=%p, "
			"user_data=%p)", __func__, connection, sender, object,
			interface, method, parameters, invocation, user_data);
	g_debug("< %s()", __func__);
}

static GVariant *g_dbus_remote_control_get_property(GDBusConnection *connection,
		const gchar *sender, const gchar *object,
		const gchar *interface, const gchar *property, GError **error,
		gpointer user_data)
{
	GVariant *ret = NULL;
	g_debug("> %s(connection=%p, sender=%s, object=%s, interface=%s, "
			"property=%s, error=%p, user_data=%p)", __func__,
			connection, sender, object, interface, property,
			error, user_data);

	if (g_strcmp0(property, "Status") == 0) {
		ret = g_variant_new_string("unconnected");
		goto out;
	}

	if (g_strcmp0(property, "Peer") == 0) {
		ret = g_variant_new_string("");
		goto out;
	}

out:
	g_debug("< %s() = %p", __func__, ret);
	return ret;
}

static gboolean g_dbus_remote_control_set_property(GDBusConnection *connection,
		const gchar *sender, const gchar *object,
		const gchar *interface, const gchar *property,
		GVariant *value, GError **error, gpointer user_data)
{
	gboolean ret = FALSE;
	g_debug("> %s(connection=%p, sender=%s, object=%s, interface=%s, "
			"property=%s, value=%p, error=%p, user_data=%p)",
			__func__, connection, sender, object, interface,
			property, value, error, user_data);
	g_debug("< %s() = %s", __func__, ret ? "TRUE" : "FALSE");
	return ret;
}

static const GDBusInterfaceVTable g_dbus_remote_control_vtable = {
	.method_call = g_dbus_remote_control_method_call,
	.get_property = g_dbus_remote_control_get_property,
	.set_property = g_dbus_remote_control_set_property,
};

static const gchar g_dbus_remote_control_xml[] =
	"<node>"
	"  <interface name=\"RemoteControl.Connection\">"
	"    <property name=\"Status\" type=\"s\" access=\"read\"/>"
	"    <property name=\"Peer\" type=\"s\" access=\"read\"/>"
	"  </interface>"
	"</node>";

static void g_dbus_bus_acquired(GDBusConnection *connection, const gchar *name,
		gpointer user_data)
{
	g_debug("> %s(connection=%p, name=%s, user_data=%p)", __func__,
			connection, name, user_data);
	g_debug("< %s()", __func__);
}

static void g_dbus_name_acquired(GDBusConnection *connection, const gchar *name,
		gpointer user_data)
{
	GDBusNodeInfo *node;
	GDBusInterfaceInfo *interface;
	guint id;

	g_debug("> %s(connection=%p, name=%s, user_data=%p)", __func__,
			connection, name, user_data);

	node = g_dbus_node_info_new_for_xml(g_dbus_remote_control_xml, NULL);
	g_debug("  node: %p", node);

	interface = g_dbus_node_info_lookup_interface(node, "RemoteControl.Connection");
	g_debug("  interface: %p", interface);
	g_debug("    name: %s", interface->name);

	id = g_dbus_connection_register_object(connection, "/RemoteControl/Connection",
			interface, &g_dbus_remote_control_vtable, NULL, NULL, NULL);
	g_debug("  id: %u", id);

	g_debug("< %s()", __func__);
}

static void g_dbus_name_lost(GDBusConnection *connection, const gchar *name,
		gpointer user_data)
{
	g_debug("> %s(connection=%p, name=%s, user_data=%p)", __func__,
			connection, name, user_data);
	g_debug("< %s()", __func__);
}
#endif /* ENABLE_DBUS */

struct signal_context {
	GMainLoop *loop;
	gint signum;
};

static struct signal_context *signal_context_new(gint signum, GMainLoop *loop)
{
	struct signal_context *context;

	context = g_new(struct signal_context, 1);
	if (!context)
		return NULL;

	context->signum = signum;
	context->loop = loop;

	return context;
}

static void signal_context_free(gpointer data)
{
	struct signal_context *context = data;

	g_free(context);
}

static gboolean handle_signal(gpointer user_data)
{
	struct signal_context *ctx = user_data;

	g_debug("interrupted by signal %d, terminating", ctx->signum);
	g_main_loop_quit(ctx->loop);

	return TRUE;
}

static gboolean setup_signal_handler(gint signum, GMainLoop *loop)
{
	struct signal_context *signal;
	GMainContext *context;
	GSource *source;

	source = g_unix_signal_source_new(signum);
	if (!source)
		return FALSE;

	signal = signal_context_new(signum, loop);
	if (!signal) {
		g_source_unref(source);
		return FALSE;
	}

	g_source_set_callback(source, handle_signal, signal,
			signal_context_free);
	context = g_main_loop_get_context(loop);
	g_source_attach(source, context);
	g_source_unref(source);

	return TRUE;
}

static gboolean setup_signal_handlers(GMainLoop *loop)
{
	static const gint signals[] = { SIGHUP, SIGINT, SIGTERM };
	guint i;

	for (i = 0; i < G_N_ELEMENTS(signals); i++) {
		if (!setup_signal_handler(signals[i], loop))
			return FALSE;
	}

	return TRUE;
}

static void on_window_destroy(GtkWidget *widget, gpointer user_data)
{
	GMainLoop *loop = user_data;
	g_main_loop_quit(loop);
}

int get_rdp_username(char *username, size_t namelen)
{
	char hostname[HOST_NAME_MAX];
	char *serial;
	int err;

	err = gethostname(hostname, sizeof(hostname));
	if (err < 0)
		return errno;

	serial = strchr(hostname, '-');
	if (!serial)
		return -EINVAL;

	return snprintf(username, namelen, "MT%s", serial + 1);
}

GtkWidget *create_rdp_window(GKeyFile *conf, GMainLoop *loop, int argc,
		char *argv[])
{
	GtkWidget *window = NULL;
	gchar *hostname = NULL;
	gchar *username = NULL;
	gchar *password = NULL;

	if (g_key_file_has_group(conf, "rdp")) {
		hostname = g_key_file_get_value(conf, "rdp", "hostname", NULL);
		username = g_key_file_get_value(conf, "rdp", "username", NULL);
		password = g_key_file_get_value(conf, "rdp", "password", NULL);
	}

	if (!hostname && (argc > 1))
		hostname = g_strdup(argv[1]);

	if (hostname) {
		guint delay = g_random_int_range(RDP_DELAY_MIN, RDP_DELAY_MAX + 1);

		if (!username || !password) {
			char buffer[HOST_NAME_MAX];
			int err;

			err = get_rdp_username(buffer, sizeof(buffer));
			if (err < 0) {
				g_print("get_rdp_username(): %s\n", strerror(-err));
				return NULL;
			}

			if (!username)
				username = g_strdup(buffer);

			if (!password)
				password = g_strdup(buffer);
		}

		window = remote_control_rdp_window_new(loop);
		gtk_window_fullscreen(GTK_WINDOW(window));
		gtk_widget_show_all(window);

		remote_control_rdp_window_connect(REMOTE_CONTROL_RDP_WINDOW(window),
				hostname, username, password, delay);

		g_free(password);
		g_free(username);
		g_free(hostname);
	}

	return window;
}

GtkWidget *create_browser_window(GKeyFile *conf, GMainLoop *loop,
		struct remote_control_data *rcd, int argc, char *argv[])
{
	gboolean check_origin = true;
	gboolean inspector = false;
	GtkWidget *window = NULL;
	const gchar *uri = NULL;

	uri = g_key_file_get_value(conf, "browser", "uri", NULL);
	if (!uri && (argc > 0))
		uri = argv[1];

	inspector = g_key_file_get_boolean(conf, "browser", "inspector", NULL);

	if (uri) {
		RemoteControlWebkitWindow *webkit;

		g_print("start with inspector: %s\n", inspector ? "enabled" : "disabled");
		window = remote_control_webkit_window_new(loop, rcd, inspector);

		webkit = REMOTE_CONTROL_WEBKIT_WINDOW(window);
		remote_control_webkit_window_load(webkit, uri);

		gtk_window_fullscreen(GTK_WINDOW(window));
		gtk_widget_show_all(window);
	}

	check_origin = g_key_file_get_boolean(conf, "browser", "check-origin", NULL);
	g_object_set(window, "check-origin", check_origin, NULL);

	return window;
}

GtkWidget *create_window(GKeyFile *conf, GMainLoop *loop,
		struct remote_control_data *rcd, int argc, char *argv[])
{
	if (g_key_file_has_group(conf, "browser"))
		return create_browser_window(conf, loop, rcd, argc, argv);

	if (g_key_file_has_group(conf, "rdp"))
		return create_rdp_window(conf, loop, argc, argv);

	return NULL;
}

static gboolean match_glob(GFile *file, gpointer user_data)
{
	const gchar *glob = user_data;
	gboolean ret = FALSE;
	gchar *filename;

	filename = g_file_get_basename(file);

	if (g_pattern_match_simple(glob, filename))
		ret = TRUE;

	g_free(filename);
	return ret;
}

static GKeyFile *g_device_tree_load_configuration(GDeviceTree *dt,
						  const gchar *directory,
						  GError **errorp)
{
	gchar **compat, **cp;
	GKeyFile *conf;
	guint count;

	conf = g_key_file_new();
	if (!conf) {
		g_set_error(errorp, G_DEVICE_TREE_ERROR,
			    G_DEVICE_TREE_ERROR_OUT_OF_MEMORY,
			    "out of memory");
		return NULL;
	}

	compat = g_device_tree_get_compatible(dt, &count);
	if (compat) {
		gchar *dir, *c;

		for (cp = compat; *cp; cp++) {
			GError *error = NULL;
			GKeyFile *file;

			/* skip vendor prefix, if any */
			c = strchr(*cp, ',');

			if (!c)
				c = *cp;
			else
				c++;

			dir = g_strdup_printf("%s/%s", directory, c);
			pr_debug("looking for configuration in %s", dir);

			file = g_key_file_new_from_directory(dir, match_glob,
							     "*.conf", &error);
			if (file) {
				if (!g_key_file_merge(conf, file, &error)) {
					pr_debug("failed to merge configuration: %s",
						 error->message);
					g_clear_error(&error);
				}

				g_key_file_free(file);
			} else {
				g_clear_error(&error);
			}

			g_free(dir);
		}

		g_strfreev(compat);
	}

	return conf;
}

static GKeyFile *remote_control_load_configuration(const gchar *filename,
						   GDeviceTree *dt,
						   GError **error)
{
	static const gchar directory[] = SYSCONF_DIR "/remote-control.conf.d";
	GError *e = NULL;
	GKeyFile *file;
	GKeyFile *conf;

	conf = g_key_file_new();
	if (!conf)
		return NULL;

	file = g_key_file_new_from_directory(directory, match_glob, "*.conf", &e);
	if (!file) {
		g_debug("failed to load configuration from directory: %s",
				e->message);
		g_clear_error(&e);
	} else {
		if (!g_key_file_merge(conf, file, &e)) {
			g_debug("failed to merge configuration: %s",
					e->message);
			g_clear_error(&e);
		}

		g_key_file_free(file);
	}

	if (dt) {
		file = g_device_tree_load_configuration(dt, directory, &e);
		if (!file) {
			pr_debug("failed to load configuration: %s",
				 e->message);
			g_clear_error(&e);
		} else {
			if (!g_key_file_merge(conf, file, &e)) {
				pr_debug("failed to merge configuration: %s",
					 e->message);
				g_clear_error(&e);
			}

			g_key_file_free(file);
		}
	}

	file = g_key_file_new_from_path(filename, G_KEY_FILE_NONE, &e);
	if (!file) {
		g_debug("failed to load `%s': %s", filename, e->message);
		g_clear_error(&e);
	} else {
		if (!g_key_file_merge(conf, file, &e)) {
			g_debug("failed to merge configuration: %s",
					e->message);
			g_clear_error(&e);
		}

		g_key_file_free(file);
	}

	return conf;
}

static gpointer remote_control_thread(gpointer data)
{
	struct remote_control_data *rcd = data;
	struct remote_control *rc;
	GMainContext *context;
	gpointer ret = NULL;
	GSource *source;
	int err;

	g_mutex_lock(&rcd->startup_mutex);
	g_cond_signal(&rcd->startup_cond);
	context = g_main_context_new();
	if (!context) {
		g_critical("failed to create main context");
		g_mutex_unlock(&rcd->startup_mutex);
		return NULL;
	}

	rcd->loop = g_main_loop_new(context, FALSE);

	err = remote_control_create(&rc, rcd->config);
	if (err < 0) {
		g_critical("remote_control_create(): %s", strerror(-err));
		g_mutex_unlock(&rcd->startup_mutex);
		return NULL;
	}

	rcd->rc = rc;

	source = remote_control_get_source(rc);
	g_assert(source != NULL);

	g_source_attach(source, context);
	g_source_unref(source);

	g_mutex_unlock(&rcd->startup_mutex);

	g_main_loop_run(rcd->loop);

	g_source_destroy(source);
	g_main_loop_unref(rcd->loop);

	return ret;
}

static struct remote_control_data *start_remote_control(GKeyFile *config,
							GError **errorp)
{
	struct remote_control_data *rcd;

	rcd = g_new0(struct remote_control_data, 1);
	if (!rcd) {
		g_set_error(errorp, G_THREAD_ERROR, 0, "out of memory");
		return NULL;
	}

	rcd->config = config;

	g_mutex_init(&rcd->startup_mutex);
	g_cond_init(&rcd->startup_cond);
	rcd->thread = g_thread_new("remote-control", remote_control_thread,
				   rcd);
	if (!rcd->thread) {
		g_set_error(errorp, G_THREAD_ERROR, 0, "cannot create thread");
		g_free(rcd);
		return NULL;
	}

	return rcd;
}

static void stop_remote_control(struct remote_control_data *rcd)
{
	g_main_loop_quit(rcd->loop);
	g_thread_join(rcd->thread);
	g_cond_clear(&rcd->startup_cond);
	g_mutex_clear(&rcd->startup_mutex);
	g_free(rcd);
}

static void on_udev_event(GUdevClient *client, gchar *action,
			  GUdevDevice *udevice, gpointer user_data)
{
	const char *caps = g_udev_device_get_property(udevice, "PRODUCT");
	/* Because we only configure the system (linphone audio-settings, see
	 * udev) to either usb-handset or classic handset on startup. We need
	 * to restart remote-control whenever somebody connects the
	 * usb-handset. */
	if (caps == NULL || !g_str_has_prefix(caps, "8bb/29c6"))
		return;
	/* In real life it only makes sense to restart remote-control if a
	 * device is added. On removal a device usually is no longer present
	 * and can thus be ignored. Additionally a restart in progress from
	 * removal could lead to missing subsequent add events if a headset
	 * is changed fast enough. */
	if (g_strcmp0(action, "add") == 0)
		g_error("Handset %s detected, restarting", action);
	else
		g_critical("Handset %s detected", action);
}

int main(int argc, char *argv[])
{
	const gchar *default_config_file = SYSCONF_DIR "/remote-control.conf";
	const gchar *const subsystems[] = { "usb/usb_interface", NULL };
	gchar *config_file = NULL;
	gboolean version = FALSE;
	GOptionEntry entries[] = {
		{ "config", 'c', 0, G_OPTION_ARG_FILENAME, &config_file,
			"Load configuration from FILENAME", "FILENAME" },
		{ "version", 'V', 0, G_OPTION_ARG_NONE, &version,
			"Print version information and exit", NULL },
		{ NULL, 0, 0, 0, NULL, NULL, NULL }
	};
	struct remote_control_data *rcd;
	GMainContext *context = NULL;
	struct watchdog *watchdog;
	GUdevClient* udev_client;
	GOptionContext *options;
	GError *error = NULL;
	GtkWidget *window;
	GDeviceTree *dt;
	GMainLoop *loop;
	GKeyFile *conf;
#ifdef ENABLE_DBUS
	guint owner;
#endif
	int err;

#ifndef __arm__
	/* Initializing threads will result in dead locks while switching vlc
	 * streams using our gles2 plugin.
	 */
	if (!XInitThreads()) {
		g_critical("XInitThreads() failed");
		return EXIT_FAILURE;
	}
#endif

#if !GLIB_CHECK_VERSION(2, 31, 0)
	if (!g_thread_supported())
		g_thread_init(NULL);
#endif

	if (!ENABLE_NLS)
		gtk_disable_setlocale();

	gtk_init(&argc, &argv);

	remote_control_log_early_init();

	options = g_option_context_new("- remote control service");
	g_option_context_add_group(options, gtk_get_option_group(TRUE));
	g_option_context_add_main_entries(options, entries, NULL);

	if (!g_option_context_parse(options, &argc, &argv, &error)) {
		g_critical("option parsing failed: %s\n", error->message);
		g_clear_error(&error);
		return EXIT_FAILURE;
	}

	g_option_context_free(options);

	if (version) {
		char *copy = strdup(argv[0]);
		g_print("%s %s\n", basename(copy), VERSION);
		free(copy);
		return EXIT_SUCCESS;
	}

	if (config_file == NULL)
		config_file = g_strdup(default_config_file);

	dt = g_device_tree_load(&error);
	if (dt) {
		gchar **models, **model;
		gchar **compat, **cp;
		guint num;

		models = g_device_tree_get_model(dt, &num);
		if (models) {
			pr_debug("%u models:", num);

			for (model = models; *model; model++)
				pr_debug("  %s", *model);

			g_strfreev(models);
		}

		compat = g_device_tree_get_compatible(dt, &num);
		if (compat) {
			pr_debug("%u compatibles:", num);

			for (cp = compat; *cp; cp++)
				pr_debug("  %s", *cp);

			g_strfreev(compat);
		}
	} else {
		pr_debug("%s", error->message);
		g_clear_error(&error);
	}

	conf = remote_control_load_configuration(config_file, dt, &error);
	if (!conf) {
		g_message("failed to load configuration: %s\n",
			  error->message);
		g_clear_error(&error);
	}

	g_device_tree_free(dt);
	g_free(config_file);

	err = remote_control_log_init(conf);
	if (err < 0) {
		g_critical("failed to initialize logging: %s\n",
			   g_strerror(-err));
		return EXIT_FAILURE;
	}

	loop = g_main_loop_new(NULL, FALSE);
	g_assert(loop != NULL);

	context = g_main_loop_get_context(loop);
	g_assert(context != NULL);

	if (!setup_signal_handlers(loop)) {
		g_critical("failed to setup signal handlers\n");
		return EXIT_FAILURE;
	}

#ifdef ENABLE_DBUS
	owner = g_bus_own_name(G_BUS_TYPE_SESSION, REMOTE_CONTROL_BUS_NAME,
			G_BUS_NAME_OWNER_FLAGS_NONE, g_dbus_bus_acquired,
			g_dbus_name_acquired, g_dbus_name_lost, NULL, NULL);
#endif

	rcd = start_remote_control(conf, &error);
	if (!rcd) {
		g_critical("failed to create control thread: %s",
			   error->message);
		g_clear_error(&error);
		return EXIT_FAILURE;
	}

	g_mutex_lock(&rcd->startup_mutex);
	while (!rcd->rc)
		g_cond_wait(&rcd->startup_cond, &rcd->startup_mutex);
	g_mutex_unlock(&rcd->startup_mutex);

	err = javascript_init(conf);
	if (err) {
		g_critical("failed to init javascript backend");
		stop_remote_control(rcd);
		return EXIT_FAILURE;
	}

	window = create_window(conf, loop, rcd, argc, argv);
	if (window) {
		g_signal_connect(G_OBJECT(window), "destroy",
				G_CALLBACK(on_window_destroy), loop);
	}

	watchdog = watchdog_new(conf, NULL);
	if (watchdog) {
		g_debug("D-Bus Watchdog activated");
		watchdog_attach(watchdog, context);
	}

	udev_client = g_udev_client_new(subsystems);
	if (udev_client) {
		g_signal_connect(udev_client, "uevent",
				 G_CALLBACK(on_udev_event), NULL);
	} else
		g_warning("init: failed to create udev client");

	g_main_loop_run(loop);

	watchdog_unref(watchdog);
	stop_remote_control(rcd);
#ifdef ENABLE_DBUS
	g_bus_unown_name(owner);
#endif
	g_object_unref(udev_client);
	g_main_loop_unref(loop);
	g_key_file_free(conf);
	remote_control_log_exit();

	return EXIT_SUCCESS;
}
