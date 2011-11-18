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

#include <linphonecore.h>
#include <glib.h>

#include "remote-control-stub.h"
#include "remote-control.h"

struct voip {
	LinphoneCore *core;
	gchar *contact;

	GThread *thread;
	bool done;
};

static void linphone_global_state_changed_cb(LinphoneCore *core, LinphoneGlobalState state, const char *message)
{
	g_print("LINPHONE: GLOBAL STATE: %s", linphone_global_state_to_string(state));

	if (message)
		g_print(" (%s)", message);

	g_print("\n");
}

static void linphone_registration_state_changed_cb(LinphoneCore *core, LinphoneProxyConfig *proxy, LinphoneRegistrationState state, const char *message)
{
	g_print("LINPHONE: REGISTRATION STATE: %p: %s", proxy, linphone_registration_state_to_string(state));

	if (message)
		g_print(" (%s)", message);

	g_print("\n");
}

static int linphone_can_accept_call(LinphoneCore *core)
{
	const MSList* calls = linphone_core_get_calls(core);
	if (calls && (ms_list_size(calls) > 1))
		return FALSE;

	return TRUE;
}

static void linphone_call_state_changed_cb(LinphoneCore *core, LinphoneCall *call, LinphoneCallState state, const char *message)
{
	struct remote_control *rc = linphone_core_get_user_data(core);
	struct event_manager *manager = remote_control_get_event_manager(rc);
	struct voip *voip = remote_control_get_voip(rc);
	const LinphoneAddress *address;
	struct event event;

	g_print("LINPHONE: CALL STATE: %p: %s", call, linphone_call_state_to_string(state));

	if (message)
		g_print(" (%s)", message);

	g_print("\n");

	memset(&event, 0, sizeof(event));
	event.source = EVENT_SOURCE_VOIP;

	switch (state) {
	case LinphoneCallIncomingReceived:
		/* FIXME: we can only accept one call at a time, so we need to
		 *        reject a second call it another is already running.
		 * In case we get two incomings there is the possibility that
		 * we have 2times the same event state which can lead to a
		 * blocked event manager. */
		if (!linphone_can_accept_call(core)) {
			g_print("LINPHONE: CALL STATE: rejecting second call\n");
			linphone_core_terminate_call(core, call);
			return;
		}

		event.voip.state = EVENT_VOIP_STATE_INCOMING;
		event_manager_report(manager, &event);

		if (voip->contact) {
			g_free(voip->contact);
			voip->contact = NULL;
		}

		address = linphone_call_get_remote_address(call);
		if (address) {
			const char *name = linphone_address_get_display_name(address);
			if (name == NULL)
				name = linphone_address_get_username(address);

			if (name)
				voip->contact = g_strdup(name);
		}
		break;

	case LinphoneCallConnected:
		event.voip.state = EVENT_VOIP_STATE_INCOMING_CONNECTED;
		event_manager_report(manager, &event);
		break;

	case LinphoneCallEnd:
		event.voip.state = EVENT_VOIP_STATE_INCOMING_DISCONNECTED;
		event_manager_report(manager, &event);
		break;

	case LinphoneCallIncomingEarlyMedia:
		break;

	case LinphoneCallOutgoingEarlyMedia:
		break;

	case LinphoneCallOutgoingProgress:
		event.voip.state = EVENT_VOIP_STATE_OUTGOING;
		event_manager_report(manager, &event);
		break;

	default:
		break;
	}
}

static void linphone_notify_presence_recv_cb(LinphoneCore *core, LinphoneFriend *friend)
{
	g_debug("> %s(core=%p, friend=%p)", __func__, core, friend);
	g_debug("< %s()", __func__);
}

static void linphone_new_subscription_request_cb(LinphoneCore *core,
		LinphoneFriend *friend, const char *url)
{
	g_debug("> %s(core=%p, friend=%p, url=%p[%s])", __func__, core, friend, url, url);
	g_debug("< %s()", __func__);
}

static void linphone_auth_info_requested_cb(LinphoneCore *core,
		const char *realm, const char *username)
{
	g_debug("> %s(core=%p, realm=%p[%s], username=%p[%s])", __func__, core,
			realm, realm, username, username);
	g_debug("< %s()", __func__);
}

static void linphone_call_log_updated_cb(LinphoneCore *core, LinphoneCallLog *log)
{
	g_debug("> %s(core=%p, log=%p)", __func__, core, log);
	g_debug("< %s()", __func__);
}

static void linphone_text_received_cb(LinphoneCore *core, LinphoneChatRoom *room, const LinphoneAddress *from, const char *message)
{
	g_debug("> %s(core=%p, room=%p, from=%p, message=%p[%s])", __func__, core, room, from, message, message);
	g_debug("< %s()", __func__);
}

static void linphone_dtmf_received_cb(LinphoneCore *core, LinphoneCall *call, int dtmf)
{
	g_debug("> %s(core=%p, call=%p, dtmf=%d)", __func__, core, call, dtmf);
	g_debug("< %s()", __func__);
}

static void linphone_refer_received_cb(LinphoneCore *core, const char *refer_to)
{
	g_debug("> %s(core=%p, refer_to=%p[%s])", __func__, core, refer_to, refer_to);
	g_debug("< %s()", __func__);
}

static void linphone_buddy_info_updated_cb(LinphoneCore *core, LinphoneFriend *friend)
{
	g_debug("> %s(core=%p, friend=%p)", __func__, core, friend);
	g_debug("< %s()", __func__);
}

static void linphone_notify_recv_cb(LinphoneCore *core, LinphoneCall *call, const char *from, const char *event)
{
	g_debug("> %s(core=%p, call=%p, from=%p[%s], event=%p[%s])", __func__, core, call, from, from, event, event);
	g_debug("< %s()", __func__);
}

static void linphone_display_status_cb(LinphoneCore *core, const char *message)
{
	g_debug("LINPHONE: STATUS: %s", message);
}

static void linphone_display_message_cb(LinphoneCore *core, const char *message)
{
	g_debug("LINPHONE: MESSAGE: %s", message);
}

static void linphone_display_warning_cb(LinphoneCore *core, const char *message)
{
	g_debug("> %s(core=%p, message=%p[%s])", __func__, core, message, message);
	g_debug("< %s()", __func__);
}

static void linphone_display_url_cb(LinphoneCore *core, const char *message,
		const char *url)
{
	g_debug("> %s(core=%p, message=%p[%s], url=%p[%s])", __func__, core,
			message, message, url, url);
	g_debug("< %s()", __func__);
}

static void linphone_show_cb(LinphoneCore *core)
{
	g_debug("> %s(core=%p)", __func__, core);
	g_debug("< %s()", __func__);
}

static const LinphoneCoreVTable vtable = {
	.global_state_changed = linphone_global_state_changed_cb,
	.registration_state_changed = linphone_registration_state_changed_cb,
	.call_state_changed = linphone_call_state_changed_cb,
	.notify_presence_recv = linphone_notify_presence_recv_cb,
	.new_subscription_request = linphone_new_subscription_request_cb,
	.auth_info_requested = linphone_auth_info_requested_cb,
	.call_log_updated = linphone_call_log_updated_cb,
	.text_received = linphone_text_received_cb,
	.dtmf_received = linphone_dtmf_received_cb,
	.refer_received = linphone_refer_received_cb,
	.buddy_info_updated = linphone_buddy_info_updated_cb,
	.notify_recv = linphone_notify_recv_cb,
	.display_status = linphone_display_status_cb,
	.display_message = linphone_display_message_cb,
	.display_warning = linphone_display_warning_cb,
	.display_url = linphone_display_url_cb,
	.show = linphone_show_cb,
};

static gpointer voip_thread(gpointer data)
{
	struct voip *voip = data;

	while (!voip->done) {
		linphone_core_iterate(voip->core);
		g_usleep(50000);
	}

	return NULL;
}

int voip_create(struct voip **voipp, struct rpc_server *server)
{
	const char *factory_config = SYSCONF_DIR "/linphone.conf";
	struct remote_control *rc = rpc_server_priv(server);
	struct voip *voip;

	if (!voipp)
		return -EINVAL;

	voip = malloc(sizeof(*voip));
	if (!voip)
		return -ENOMEM;

	memset(voip, 0, sizeof(*voip));

	voip->core = linphone_core_new(&vtable, NULL, factory_config, rc);
	if (!voip->core) {
		free(voip);
		return -ENOMEM;
	}

	linphone_core_set_ring(voip->core, NULL);
	/* FIXME: For now set playback volume to 100% so we can ajust the
	 *        volume later using the default mixer settings.
	 *        In the future this needs to be a configurable value */
	linphone_core_set_playback_gain_db(voip->core, 1.0f);
	// maybe we need this as well, but it looks like this is just the
	// volume of the soundcard. which we can control via alsa.
	//linphone_core_set_play_level(voip->core, 100);

	voip->thread = g_thread_create(voip_thread, voip, TRUE, NULL);
	if (!voip->thread) {
		g_error("failed to create thread");
		return -ENOMEM;
	}

	*voipp = voip;
	return 0;
}

int voip_free(struct voip *voip)
{
	if (!voip)
		return -EINVAL;

	voip->done = true;
	g_thread_join(voip->thread);

	g_free(voip->contact);
	linphone_core_terminate_all_calls(voip->core);
	linphone_core_destroy(voip->core);
	free(voip);
	return 0;
}

GSource *voip_get_source(struct voip *voip)
{
	return NULL;
}

static int is_valid_string(const char* str)
{
	if (!str)
		return FALSE;

	return strlen(str) > 0;
}

int voip_login(struct voip *voip, const char *host, uint16_t port,
		const char *username, const char *password)
{
	LinphoneProxyConfig *proxy;
	const char *domain = host;
	LinphoneAddress *address;
	bool use_default = true;
	LinphoneAuthInfo *auth;
	char *server = NULL;
	char *user = NULL;
	char *identity;
	int len;

	if (!voip)
		return -EINVAL;

	if (!port || !is_valid_string(host) || !is_valid_string(username) ||
	    !is_valid_string(password))
	{
		linphone_core_get_default_proxy(voip->core, &proxy);
		if (proxy) {
			if (!linphone_proxy_config_is_registered(proxy)) {
				linphone_proxy_config_edit(proxy);
				linphone_proxy_config_enable_register(proxy, TRUE);
				linphone_proxy_config_done(proxy);
			}
		} else {
			return -EIO;
		}

		return 0;
	}

	linphone_core_get_default_proxy(voip->core, &proxy);
	if (!proxy) {
		proxy = linphone_proxy_config_new();
		if (!proxy)
			return -ENOMEM;

		use_default = false;
	} else {
		linphone_proxy_config_edit(proxy);
	}

	len = strlen(host) + strlen("sip:");
	server = malloc(len + 1);
	if (!server)
		return -ENOMEM;

	snprintf(server, len + 1, "sip:%s", host);

	user = strdup(username);
	if (!user) {
		free(server);
		return -ENOMEM;
	}

	if (strchr(username, '@') != NULL) {
		char *ptr = strchr(user, '@');
		domain = ptr + 1;
		ptr[0] = '\0';
	}

	address = linphone_address_new(server);
	linphone_address_set_display_name(address, user);
	linphone_address_set_username(address, user);
	linphone_address_set_domain(address, domain);
	linphone_address_set_port_int(address, port);
	identity = linphone_address_as_string(address);
	linphone_address_destroy(address);

	auth = linphone_auth_info_new(user, NULL, password, NULL, NULL);
	linphone_core_add_auth_info(voip->core, auth);

	linphone_proxy_config_set_server_addr(proxy, server);
	linphone_proxy_config_set_identity(proxy, identity);
	linphone_proxy_config_enable_register(proxy, TRUE);

	if (!use_default) {
		linphone_core_add_proxy_config(voip->core, proxy);
		linphone_core_set_default_proxy(voip->core, proxy);
	} else {
		linphone_proxy_config_done(proxy);
	}

	ms_free(identity);
	free(server);
	free(user);

	return 0;
}

int voip_logout(struct voip *voip)
{
	LinphoneProxyConfig *proxy = NULL;

	if (!voip)
		return -EINVAL;

	linphone_core_get_default_proxy(voip->core, &proxy);
	if (proxy && linphone_proxy_config_is_registered(proxy)) {
		int err;
		linphone_proxy_config_edit(proxy);
		linphone_proxy_config_enable_register(proxy, FALSE);
		err = linphone_proxy_config_done(proxy);
		g_debug("linphone_proxy_config_done(): %d", err);
	}

	return 0;
}

int voip_call(struct voip *voip, const char *uri)
{
	LinphoneCallParams *params;
	LinphoneCall *call;

	if (!voip)
		return -EINVAL;

	params = linphone_core_create_default_call_parameters(voip->core);
	linphone_call_params_enable_early_media_sending(params, TRUE);

	call = linphone_core_invite_with_params(voip->core, uri, params);
	if (!call) {
		linphone_call_params_destroy(params);
		return -EIO;
	}

	linphone_call_params_destroy(params);
	return 0;
}

int voip_accept(struct voip *voip, char **caller)
{
	int err;

	if (!voip)
		return -EINVAL;

	err = linphone_core_accept_call(voip->core, NULL);
	if (err < 0)
		return err;

	if (caller && voip->contact)
		*caller = g_strdup(voip->contact);

	return 0;
}

int voip_terminate(struct voip *voip)
{
	int err;

	if (!voip)
		return -EINVAL;

	err = linphone_core_terminate_call(voip->core, NULL);
	if (err < 0)
		return err;

	return 0;
}

int voip_get_state(struct voip *voip, enum voip_state *statep)
{
	enum voip_state state = VOIP_STATE_LOGGED_OUT;
	LinphoneProxyConfig *proxy = NULL;

	if (!voip || !statep)
		return -EINVAL;

	linphone_core_get_default_proxy(voip->core, &proxy);
	if (proxy && linphone_proxy_config_is_registered(proxy))
		state = VOIP_STATE_LOGGED_IN;

	*statep = state;
	return 0;
}

int voip_get_contact(struct voip *voip, const char **contactp)
{
	if (!voip || !contactp)
		return -EINVAL;

	*contactp = voip->contact;
	return 0;
}

int voip_dial(struct voip *voip, uint8_t dtmf)
{
	if (!voip)
		return -EINVAL;

	/* we can only send dtmf tone on running calls */
	if (!linphone_core_in_call(voip->core))
		return -ENOSYS;

	linphone_core_send_dtmf(voip->core, (char)dtmf);
	return 0;
}
