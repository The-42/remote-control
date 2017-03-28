/*
 * Copyright (C) 2017 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <netdb.h>
#include <glib.h>

#include "remote-control-rpc.h"
#include "remote-control-stub.h"
#include "remote-control.h"
#include "rcrpc.h"

enum remote_control_state {
	REMOTE_CONTROL_UNCONNECTED,
	REMOTE_CONTROL_CONNECTED,
	REMOTE_CONTROL_IDLE,
	REMOTE_CONTROL_DISCONNECTED,
};

struct rpc_source {
	GSource source;

	struct rpc_server *server;
	enum remote_control_state state;
	char peer[NI_MAXHOST + 1];
	GPollFD poll_listen;
	GPollFD poll_client;
};

static int rpc_log(int priority, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = vprintf(fmt, ap);
	va_end(ap);

	return ret;
}

static int rcrpc_free(struct rpc_server *server)
{
	if (!server)
		return -EINVAL;

	rpc_irq_cleanup();
	rpc_net_cleanup();
	rpc_server_free(server);

	return 0;
}

static int remote_control_dispatch(struct rpc_server *server,
		struct rpc_packet *request)
{
	return rpc_dispatch(server, request);
}

static gboolean rpc_source_prepare(GSource *source, gint *timeout)
{
	struct rpc_source *src = (struct rpc_source *)source;
	struct rpc_server *server = src->server;
	int err;

	switch (src->state) {
	case REMOTE_CONTROL_UNCONNECTED:
		break;

	case REMOTE_CONTROL_CONNECTED:
		err = rpc_server_get_client_socket(server);
		if (err < 0) {
			g_log(G_LOG_DOMAIN, G_LOG_LEVEL_ERROR,
					"rpc_server_get_client_socket(): %s",
					strerror(-err));
			src->state = REMOTE_CONTROL_UNCONNECTED;
			break;
		}

		src->poll_client.events = G_IO_IN | G_IO_HUP | G_IO_ERR;
		src->poll_client.fd = err;
		g_source_add_poll(source, &src->poll_client);

		src->state = REMOTE_CONTROL_IDLE;
		break;

	case REMOTE_CONTROL_IDLE:
		break;

	case REMOTE_CONTROL_DISCONNECTED:
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

static gboolean rpc_source_check(GSource *source)
{
	struct rpc_source *src = (struct rpc_source *)source;

	/* handle server socket */
	if ((src->poll_listen.revents & G_IO_HUP) ||
	    (src->poll_listen.revents & G_IO_ERR))
		return FALSE;

	if (src->poll_listen.revents & G_IO_IN)
		return TRUE;

	/* handle client socket */
	if ((src->poll_client.revents & G_IO_HUP) ||
	    (src->poll_client.revents & G_IO_ERR)) {
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO, "connection closed by "
				"%s", src->peer);
		src->state = REMOTE_CONTROL_DISCONNECTED;
		return TRUE;
	}

	if (src->poll_client.revents & G_IO_IN)
		return TRUE;

	return FALSE;
}

static gboolean rpc_source_dispatch(GSource *source, GSourceFunc callback, gpointer user_data)
{
	struct rpc_source *src = (struct rpc_source *)source;
	struct rpc_server *server = src->server;
	struct rpc_packet *request = NULL;
	struct sockaddr *addr = NULL;
	gboolean ret = TRUE;
	int err;

	switch (src->state) {
	case REMOTE_CONTROL_UNCONNECTED:
		err = rpc_server_accept(server);
		if (err < 0) {
			g_log(G_LOG_DOMAIN, G_LOG_LEVEL_ERROR,
					"rpc_server_accept(): %s",
					strerror(-err));
			break;
		}

		err = rpc_server_get_peer(server, &addr);
		if ((err > 0) && addr) {
			err = getnameinfo(addr, err, src->peer, NI_MAXHOST,
					NULL, 0, NI_NUMERICHOST);
			if (!err) {
				g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
						"connection accepted from %s",
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
		if ((err < 0) && (err != -ECONNRESET)) {
			g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
					"rpc_server_recv(): %s",
					strerror(-err));
			ret = FALSE;
			break;
		}

		if ((err == 0) || (err == -ECONNRESET)) {
			g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO, "connection "
					"closed by %s", src->peer);
			src->state = REMOTE_CONTROL_DISCONNECTED;
			break;
		}

		err = remote_control_dispatch(server, request);
		if (err == -ENOSYS) {
			uint32_t opcode = 0;
			rpc_packet_get_opcode(request, &opcode);
			g_critical("rpc_dispatch(): opcode %u is not "
					"implemented (anymore?)",
					opcode);
		} else if (err < 0) {
			g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
					"rpc_dispatch(): %s", strerror(-err));
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

static void rpc_source_finalize(GSource *source)
{
	struct rpc_source *src = (struct rpc_source *)source;
	rcrpc_free(src->server);
}

static GSourceFuncs rpc_source_funcs = {
	.prepare = rpc_source_prepare,
	.check = rpc_source_check,
	.dispatch = rpc_source_dispatch,
	.finalize = rpc_source_finalize,
};

static int socket_enable_keepalive(int socket, gboolean enable)
{
	int optval = enable ? 1 : 0;

	return setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, &optval,
			 sizeof(optval));
}

static gboolean config_get_socket_keepalive(GKeyFile *config)
{
	GError *error = NULL;
	gboolean enable;

	g_return_val_if_fail(config != NULL, false);

	enable = g_key_file_get_boolean(config, "general",
					"rpc-socket-keepalive", &error);
	if (error != NULL) {
		g_clear_error(&error);
		enable = false;
	}

	return enable;
}

int rcrpc_create(void *rcpriv, GKeyFile *config)
{
	struct remote_control *rc = (struct remote_control *)rcpriv;
	struct remote_control **rpc_priv;
	struct rpc_server *server;
	struct rpc_source *src;
	GSource *source;
	int err, socket;

	if (!rc)
		return -EINVAL;

	err = rpc_server_create(&server, NULL, sizeof(struct remote_control *));
	if (err < 0) {
		g_critical("rpc_server_create(): %s", strerror(-err));
		return err;
	}

	err = rpc_server_listen(server, 7482);
	if (err < 0) {
		g_critical("rpc_server_listen(): %s", strerror(-err));
		return err;
	}

	rpc_priv = rpc_server_priv(server);
	*rpc_priv = rc;

	source = g_source_new(&rpc_source_funcs, sizeof(struct rpc_source));
	if (!source) {
		g_critical("g_source_new() failed");
		return -ENOMEM;
	}

	src = (struct rpc_source *)source;
	src->server = server;

	socket = rpc_server_get_listen_socket(server);
	if (socket < 0) {
		g_critical("rpc_server_get_listen(): %s", g_strerror(-socket));
		return socket;
	}

	err = socket_enable_keepalive(socket,
				      config_get_socket_keepalive(config));
	if (err < 0) {
		g_critical("socket_enable_keepalive(): %s", g_strerror(-err));
		return err;
	}

	src->poll_listen.events = G_IO_IN | G_IO_HUP | G_IO_ERR;
	src->poll_listen.fd = socket;
	g_source_add_poll(source, &src->poll_listen);
	g_source_add_child_source(remote_control_get_source(rc), source);
	g_source_unref(source);

	rpc_irq_init(rpc_priv);

	return 0;
}
