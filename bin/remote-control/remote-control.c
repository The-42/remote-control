/*
 * Copyright (C) 2010-2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <librpc.h>
#include <glib.h>

#include "remote-control-window.h"
#include "remote-control.h"

static const gchar REMOTE_CONTROL_BUS_NAME[] = "de.avionic-design.RemoteControl";

static GMainLoop *g_loop = NULL;

enum remote_control_state {
	REMOTE_CONTROL_UNCONNECTED,
	REMOTE_CONTROL_CONNECTED,
	REMOTE_CONTROL_IDLE,
	REMOTE_CONTROL_DISCONNECTED,
};

struct remote_control_source {
	GSource source;

	enum remote_control_state state;
	char peer[NI_MAXHOST + 1];

	struct remote_control *rc;
	GPollFD poll_listen;
	GPollFD poll_client;
};

static inline struct remote_control_source *REMOTE_CONTROL_SOURCE(GSource *source)
{
	return (struct remote_control_source *)source;
}

#ifdef ENABLE_DBUS
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

static int rpc_log(int priority, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = vprintf(fmt, ap);
	va_end(ap);

	return ret;
}

static gboolean g_remote_control_source_prepare(GSource *source, gint *timeout)
{
	struct remote_control_source *src = REMOTE_CONTROL_SOURCE(source);
	struct rpc_server *server = rpc_server_from_priv(src->rc);
	int err;

	switch (src->state) {
	case REMOTE_CONTROL_UNCONNECTED:
		//g_debug("state: unconnected");
		break;

	case REMOTE_CONTROL_CONNECTED:
		//g_debug("state: connected");
		err = rpc_server_get_client_socket(server);
		if (err < 0) {
			g_debug("rpc_server_get_client_socket(): %s", strerror(-err));
			src->state = REMOTE_CONTROL_UNCONNECTED;
			break;
		}

		src->poll_client.events = G_IO_IN | G_IO_HUP | G_IO_ERR;
		src->poll_client.fd = err;
		g_source_add_poll(source, &src->poll_client);

		src->state = REMOTE_CONTROL_IDLE;
		break;

	case REMOTE_CONTROL_IDLE:
		//g_debug("state: idle");
		break;

	case REMOTE_CONTROL_DISCONNECTED:
		//g_debug("state: disconnected");
		g_source_remove_poll(source, &src->poll_client);
		src->poll_client.events = 0;
		src->poll_client.fd = -1;
		src->state = REMOTE_CONTROL_UNCONNECTED;
		break;
	}

	if (timeout)
		*timeout = -1;

	return FALSE;
}

static gboolean g_remote_control_source_check(GSource *source)
{
	struct remote_control_source *src = REMOTE_CONTROL_SOURCE(source);

	/* handle server socket */
	if ((src->poll_listen.revents & G_IO_HUP) ||
	    (src->poll_listen.revents & G_IO_ERR)) {
		g_error("  listen: G_IO_HUP | G_IO_ERR");
		return FALSE;
	}

	if (src->poll_listen.revents & G_IO_IN) {
		g_debug("  listen: G_IO_IN");
		return TRUE;
	}

	/* handle client socket */
	if ((src->poll_client.revents & G_IO_HUP) ||
	    (src->poll_client.revents & G_IO_ERR)) {
		g_debug("%s(): connection closed by %s", __func__, src->peer);
		src->state = REMOTE_CONTROL_UNCONNECTED;
		return TRUE;
	}

	if (src->poll_client.revents & G_IO_IN) {
		g_debug("  client: G_IO_IN (state: %d)", src->state);
		return TRUE;
	}

	return FALSE;
}

static gboolean g_remote_control_source_dispatch(GSource *source, GSourceFunc callback, gpointer user_data)
{
	struct remote_control_source *src = REMOTE_CONTROL_SOURCE(source);
	struct rpc_server *server = rpc_server_from_priv(src->rc);
	struct rpc_packet *request = NULL;
	struct sockaddr *addr = NULL;
	gboolean ret = TRUE;
	int err;

	switch (src->state) {
	case REMOTE_CONTROL_UNCONNECTED:
		err = rpc_server_accept(server);
		if (err < 0) {
			g_debug("rpc_server_accept(): %s", strerror(-err));
			break;
		}

		err = rpc_server_get_peer(server, &addr);
		if ((err > 0) && addr) {
			err = getnameinfo(addr, err, src->peer, NI_MAXHOST,
					NULL, 0, NI_NUMERICHOST);
			if (!err) {
				g_debug("connection accepted from %s",
						src->peer);
			}

			free(addr);
		}

		src->state = REMOTE_CONTROL_CONNECTED;
		break;

	case REMOTE_CONTROL_CONNECTED:
		break;

	case REMOTE_CONTROL_IDLE:
		err = rpc_server_recv(server, &request);
		if (err < 0) {
			g_debug("rpc_server_recv(): %s", strerror(-err));
			ret = FALSE;
			break;
		}

		if (err == 0) {
			g_debug("%s(): connection closed by %s", __func__, src->peer);
			src->state = REMOTE_CONTROL_DISCONNECTED;
			break;
		}

		err = remote_control_dispatch(server, request);
		if (err < 0) {
			g_debug("rpc_dispatch(): %s", strerror(-err));
			rpc_packet_dump(request, rpc_log, 0);
			rpc_packet_free(request);
			ret = FALSE;
			break;
		}

		rpc_packet_free(request);
		break;

	case REMOTE_CONTROL_DISCONNECTED:
		break;
	}

	return ret;
}

static void g_remote_control_source_finalize(GSource *source)
{
	struct remote_control_source *src = REMOTE_CONTROL_SOURCE(source);

	g_debug("> %s(source=%p)", __func__, source);

	remote_control_free(src->rc);

	g_debug("< %s()", __func__);
}

static GSourceFuncs g_remote_control_source = {
	.prepare = g_remote_control_source_prepare,
	.check = g_remote_control_source_check,
	.dispatch = g_remote_control_source_dispatch,
	.finalize = g_remote_control_source_finalize,
};

static GSource *g_remote_control_source_new(GMainLoop *loop)
{
	struct remote_control_source *src = NULL;
	struct rpc_server *server;
	GMainContext *context;
	GSource *source;
	int err;

	source = g_source_new(&g_remote_control_source, sizeof(*src));
	if (!source) {
		g_error("failed to allocate source");
		return NULL;
	}

	src = REMOTE_CONTROL_SOURCE(source);

	err = remote_control_create(&src->rc);
	if (err < 0) {
		g_error("remote_control_create(): %s", strerror(-err));
		g_source_unref(source);
		return NULL;
	}

	server = rpc_server_from_priv(src->rc);

	err = rpc_server_get_listen_socket(server);
	if (err < 0) {
		g_error("rpc_server_get_listen_socket(): %s", strerror(-err));
		g_source_unref(source);
		return NULL;
	}

	src->poll_listen.events = G_IO_IN | G_IO_HUP | G_IO_ERR;
	src->poll_listen.fd = err;

	g_source_add_poll(source, &src->poll_listen);

	context = g_main_loop_get_context(loop);
	g_assert(context != NULL);

	g_source_attach(source, context);
	g_source_unref(source);

	return source;
}

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

int main(int argc, char *argv[])
{
	const gchar *conffile = SYSCONF_DIR "/remote-control.conf";
	GOptionContext *context;
	gchar *hostname = NULL;
	gchar *username = NULL;
	gchar *password = NULL;
	GtkWidget *window;
	GMainLoop *loop;
	GSource *source;
	GKeyFile *conf;
	GError *error;
#ifdef ENABLE_DBUS
	guint owner;
#endif

	if (g_thread_supported())
		g_thread_init(NULL);

	if (!setup_signal_handler()) {
		g_print("failed to setup signal handler\n");
		return 1;
	}

	conf = g_key_file_new();
	if (!conf) {
		g_print("failed to create key file\n");
		return EXIT_FAILURE;
	}

	context = g_option_context_new("- remote control service");
	g_option_context_add_group(context, gtk_get_option_group(TRUE));

	if (!g_option_context_parse(context, &argc, &argv, &error)) {
		g_print("option parsing failed: %s\n", error->message);
		return 1;
	}

	g_option_context_free(context);

	if (!g_key_file_load_from_file(conf, conffile, G_KEY_FILE_NONE, NULL))
		g_warning("failed to load configuration file %s", conffile);

	loop = g_loop = g_main_loop_new(NULL, FALSE);
	g_assert(loop != NULL);

#ifdef ENABLE_DBUS
	owner = g_bus_own_name(G_BUS_TYPE_SESSION, REMOTE_CONTROL_BUS_NAME,
			G_BUS_NAME_OWNER_FLAGS_NONE, g_dbus_bus_acquired,
			g_dbus_name_acquired, g_dbus_name_lost, NULL, NULL);
#endif

	source = g_remote_control_source_new(loop);
	g_assert(source != NULL);

	if (g_key_file_has_group(conf, "rdp")) {
		hostname = g_key_file_get_value(conf, "rdp", "hostname", NULL);
		username = g_key_file_get_value(conf, "rdp", "username", NULL);
		password = g_key_file_get_value(conf, "rdp", "password", NULL);
	}

	if (!hostname && (argc > 1))
		hostname = g_strdup(argv[1]);

	if (hostname) {
		if (!username || !password) {
			char buffer[HOST_NAME_MAX];
			int err;

			err = get_rdp_username(buffer, sizeof(buffer));
			if (err < 0) {
				g_print("get_rdp_username(): %s\n", strerror(-err));
				return 1;
			}

			if (!username)
				username = g_strdup(buffer);

			if (!password)
				password = g_strdup(buffer);
		}

		window = remote_control_window_new(loop);
		g_signal_connect(G_OBJECT(window), "destroy",
				G_CALLBACK(on_window_destroy), loop);
		gtk_window_fullscreen(GTK_WINDOW(window));
		gtk_widget_show_all(window);

		remote_control_window_connect(REMOTE_CONTROL_WINDOW(window),
				hostname, username, password);

		g_free(password);
		g_free(username);
		g_free(hostname);
	}

	g_main_loop_run(loop);

	g_source_destroy(source);
#ifdef ENABLE_DBUS
	g_bus_unown_name(owner);
#endif
	g_main_loop_unref(loop);
	g_key_file_free(conf);
	return 0;
}
