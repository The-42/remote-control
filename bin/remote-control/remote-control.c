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
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>
#include <librpc.h>
#include <glib.h>

#include "remote-control-webkit-window.h"
#include "remote-control-rdp-window.h"
#include "remote-control.h"

static GMainLoop *g_loop = NULL;

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

static void handle_signal(int signum)
{
	g_debug("signal: %d", signum);
	g_main_loop_quit(g_loop);
}

gboolean setup_signal_handler(void)
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = handle_signal;

	if (sigaction(SIGHUP, &sa, NULL) < 0)
		return FALSE;

	if (sigaction(SIGINT, &sa, NULL) < 0)
		return FALSE;

	if (sigaction(SIGTERM, &sa, NULL) < 0)
		return FALSE;

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

GtkWidget *create_rdp_window(GKeyFile *conf, GMainContext *context, int argc,
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

		window = remote_control_rdp_window_new(context);
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

GtkWidget *create_browser_window(GKeyFile *conf, GMainContext *context,
		int argc, char *argv[])
{
	GtkWidget *window = NULL;
	const gchar *uri = NULL;

	uri = g_key_file_get_value(conf, "browser", "uri", NULL);
	if (!uri && (argc > 0))
		uri = argv[1];

	if (uri) {
		RemoteControlWebkitWindow *webkit;

		window = remote_control_webkit_window_new(context);
		webkit = REMOTE_CONTROL_WEBKIT_WINDOW(window);
		remote_control_webkit_window_load(webkit, uri);

		gtk_window_fullscreen(GTK_WINDOW(window));
		gtk_widget_show_all(window);
	}

	return window;
}

GtkWidget *create_window(GKeyFile *conf, GMainContext *context, int argc,
		char *argv[])
{
	if (g_key_file_has_group(conf, "browser"))
		return create_browser_window(conf, context, argc, argv);

	if (g_key_file_has_group(conf, "rdp"))
		return create_rdp_window(conf, context, argc, argv);

	return NULL;
}

int main(int argc, char *argv[])
{
	const gchar *default_config_file = SYSCONF_DIR "/remote-control.conf";
	gchar *config_file = NULL;
	gboolean version = FALSE;
	GOptionEntry entries[] = {
		{ "config", 'c', 0, G_OPTION_ARG_FILENAME, &config_file,
			"Load configuration from FILENAME", "FILENAME" },
		{ "version", 'V', 0, G_OPTION_ARG_NONE, &version,
			"Print version information and exit", NULL },
		{ NULL, 0, 0, 0, NULL, NULL, NULL }
	};
	struct remote_control *rc;
	GOptionContext *options;
	GMainContext *context;
	GError *error = NULL;
	GtkWidget *window;
	GMainLoop *loop;
	GSource *source;
	GKeyFile *conf;
#ifdef ENABLE_DBUS
	guint owner;
#endif
	int err;

	if (!XInitThreads())
		g_debug("XInitThreads() failed");

	if (g_thread_supported())
		g_thread_init(NULL);

	gtk_init(&argc, &argv);

	options = g_option_context_new("- remote control service");
	g_option_context_add_group(options, gtk_get_option_group(TRUE));
	g_option_context_add_main_entries(options, entries, NULL);

	if (!g_option_context_parse(options, &argc, &argv, &error)) {
		g_print("option parsing failed: %s\n", error->message);
		return EXIT_FAILURE;
	}

	g_option_context_free(options);

	if (version) {
		char *copy = strdup(argv[0]);
		printf("%s %s\n", basename(copy), VERSION);
		free(copy);
		return EXIT_SUCCESS;
	}

	if (!setup_signal_handler()) {
		g_print("failed to setup signal handler\n");
		return EXIT_FAILURE;
	}

	conf = g_key_file_new();
	if (!conf) {
		g_print("failed to create key file\n");
		return EXIT_FAILURE;
	}

	if (config_file == NULL)
		config_file = g_strdup(default_config_file);

	if (!g_key_file_load_from_file(conf, config_file, G_KEY_FILE_NONE, NULL))
		g_warning("failed to load configuration file %s", config_file);

	loop = g_loop = g_main_loop_new(NULL, FALSE);
	g_assert(loop != NULL);

	context = g_main_loop_get_context(loop);
	g_assert(context != NULL);

#ifdef ENABLE_DBUS
	owner = g_bus_own_name(G_BUS_TYPE_SESSION, REMOTE_CONTROL_BUS_NAME,
			G_BUS_NAME_OWNER_FLAGS_NONE, g_dbus_bus_acquired,
			g_dbus_name_acquired, g_dbus_name_lost, NULL, NULL);
#endif

	err = remote_control_create(&rc);
	if (err < 0) {
		g_error("remote_control_create(): %s", strerror(-err));
		return EXIT_FAILURE;
	}

	window = create_window(conf, context, argc, argv);
	if (window) {
		g_signal_connect(G_OBJECT(window), "destroy",
				G_CALLBACK(on_window_destroy), loop);
	}

	source = remote_control_get_source(rc);
	g_assert(source != NULL);

	g_source_attach(source, context);
	g_source_unref(source);

	g_main_loop_run(loop);

	g_source_destroy(source);
#ifdef ENABLE_DBUS
	g_bus_unown_name(owner);
#endif
	g_main_loop_unref(loop);
	g_key_file_free(conf);
	g_free(config_file);
	return EXIT_SUCCESS;
}
