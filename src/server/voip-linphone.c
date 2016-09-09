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

#include <ctype.h>
#include <linphonecore.h>
#include <lpconfig.h>
#include <glib.h>

#include "remote-control-stub.h"
#include "remote-control.h"

struct voip {
	GSource *timeout;
	LinphoneCore *core;
	gchar *contact_name;
	gchar *contact_display;
	int expires;

	voip_onstatechange_cb onstatechange_cb;
	void *callback_data;
	void *callback_owner;
};

static void linphone_global_state_changed_cb(LinphoneCore *core,
		LinphoneGlobalState state, const char *message)
{
	const char *name = linphone_global_state_to_string(state);

	g_debug("voip-linphone: global state changed to %s: %s",
				name, message ?: "");
}

static void linphone_registration_state_changed_cb(LinphoneCore *core,
		LinphoneProxyConfig *proxy, LinphoneRegistrationState state,
		const char *message)
{
	const char *name = linphone_registration_state_to_string(state);

	g_debug("voip-linphone: registration state on proxy %p "
		"changed to %s: %s", proxy, name, message ?: "");
}

static void voip_update_contact(struct voip *voip, LinphoneCall *call)
{
	const LinphoneAddress *address;
	const char *contact_display;
	const char *contact_name;

	if (voip->contact_name) {
		g_free(voip->contact_name);
		voip->contact_name = NULL;
	}
	if (voip->contact_display) {
		g_free(voip->contact_display);
		voip->contact_display = NULL;
	}

	address = linphone_call_get_remote_address(call);
	if (!address)
		return;

	contact_name = linphone_address_get_username(address);
	if (contact_name)
		voip->contact_name = g_strdup(contact_name);

	contact_display = linphone_address_get_display_name(address);
	if (contact_display)
		voip->contact_display = g_strdup(contact_display);
}

static void linphone_call_state_changed_cb(LinphoneCore *core,
		LinphoneCall *call, LinphoneCallState state,
		const char *message)
{
	struct remote_control *rc = linphone_core_get_user_data(core);
	struct voip *voip = remote_control_get_voip(rc);
	enum voip_state cb_state = VOIP_STATE_IDLE;
	const char *name;

	name = linphone_call_state_to_string(state);

	g_debug("voip-linphone: call state changed to %s: %s", name,
			message ?: "");

	switch (state) {
	case LinphoneCallIncomingReceived:
		/* reject incoming calls if a call is already active */
		if (!linphone_core_in_call(core)) {
			g_debug("voip-linphone: busy, rejecting call\n");
			linphone_core_decline_call(core, call, LinphoneReasonDeclined);
			return;
		}

		cb_state = VOIP_STATE_INCOMING;

		voip_update_contact(voip, call);
		break;

	case LinphoneCallConnected:
		cb_state = VOIP_STATE_CONNECTED;
		break;

	case LinphoneCallEnd:
		cb_state = VOIP_STATE_DISCONNECTED;
		break;

	case LinphoneCallIncomingEarlyMedia:
		cb_state = VOIP_STATE_INCOMING_EARLYMEDIA;
		break;

	case LinphoneCallOutgoingEarlyMedia:
		cb_state = VOIP_STATE_OUTGOING_EARLYMEDIA;
		break;

	case LinphoneCallOutgoingProgress:
		cb_state = VOIP_STATE_OUTGOING;
		break;

	case LinphoneCallError:
		/* Sadly there is no other way to get the info, that the called
		 * user is busy. */
		if (g_strcmp0(message, "Busy Here") == 0)
			cb_state = VOIP_STATE_ERROR_USER_BUSY;
		else
			cb_state = VOIP_STATE_OUTGOING_FAILED;
		break;

	case LinphoneCallIdle:
		cb_state = VOIP_STATE_IDLE;
		break;

	default:
		g_debug("voip-linphone: unhandled event: %s", name);
		return;
	}

	if (voip->onstatechange_cb)
		voip->onstatechange_cb(cb_state, voip->callback_data);
	else
		g_debug("voip-linphone: no callback set. You miss this event!");

}

static void linphone_notify_presence_received_cb(LinphoneCore *core,
		LinphoneFriend *friend)
{
	g_debug("voip-linphone: presence notification for %p", friend);
}

static void linphone_new_subscription_requested_cb(LinphoneCore *core,
		LinphoneFriend *friend, const char *url)
{
	g_debug("voip-linphone: subscription request from %p: %s", friend,
			url);
}

static void linphone_auth_info_requested_cb(LinphoneCore *core,
		const char *realm, const char *username, const char *domain)
{
	g_debug("voip-linphone: authorization info requested for %s@%s %s",
			username, realm, domain);
}

static void linphone_call_log_updated_cb(LinphoneCore *core,
		LinphoneCallLog *log)
{
	g_debug("voip-linphone: call log update: %p", log);
}

static void linphone_text_received_cb(LinphoneCore *core,
		LinphoneChatRoom *room, const LinphoneAddress *from,
		const char *message)
{
	g_debug("voip-linphone: text received from %p/%p: %s", room, from,
			message);
}

static void linphone_dtmf_received_cb(LinphoneCore *core, LinphoneCall *call,
		int dtmf)
{
	g_debug("voip-linphone: dtmf received: %02x, '%c'", dtmf,
			isprint(dtmf) ? dtmf : '?');
}

static void linphone_refer_received_cb(LinphoneCore *core,
		const char *refer_to)
{
	g_debug("voip-linphone: referring call to %s", refer_to);
}

static void linphone_buddy_info_updated_cb(LinphoneCore *core,
		LinphoneFriend *friend)
{
	g_debug("voip-linphone: buddy info updated for %p", friend);
}

static void linphone_notify_received_cb(LinphoneCore *core, LinphoneEvent *evt,
		const char *notified_event, const LinphoneContent *body)
{
	g_debug("voip-linphone: notification received: %s", notified_event);
}

static void linphone_display_status_cb(LinphoneCore *core,
		const char *message)
{
	g_debug("voip-linphone: status: %s", message);
}

static void linphone_display_message_cb(LinphoneCore *core,
		const char *message)
{
	g_debug("voip-linphone: message: %s", message);
}

static void linphone_display_warning_cb(LinphoneCore *core,
		const char *message)
{
	g_debug("voip-linphone: warning: %s", message);
}

static void linphone_display_url_cb(LinphoneCore *core, const char *message,
		const char *url)
{
	g_debug("voip-linphone: url: %s: %s", message, url);
}

static void linphone_show_cb(LinphoneCore *core)
{
	g_debug("voip-linphone: show notification received");
}

static const LinphoneCoreVTable vtable = {
	.global_state_changed = linphone_global_state_changed_cb,
	.registration_state_changed = linphone_registration_state_changed_cb,
	.call_state_changed = linphone_call_state_changed_cb,
	.notify_presence_received = linphone_notify_presence_received_cb,
	.new_subscription_requested = linphone_new_subscription_requested_cb,
	.auth_info_requested = linphone_auth_info_requested_cb,
	.call_log_updated = linphone_call_log_updated_cb,
	.text_received = linphone_text_received_cb,
	.dtmf_received = linphone_dtmf_received_cb,
	.refer_received = linphone_refer_received_cb,
	.buddy_info_updated = linphone_buddy_info_updated_cb,
	.notify_received = linphone_notify_received_cb,
	.display_status = linphone_display_status_cb,
	.display_message = linphone_display_message_cb,
	.display_warning = linphone_display_warning_cb,
	.display_url = linphone_display_url_cb,
	.show = linphone_show_cb,
};

static void linphone_log(OrtpLogLevel level, const char *fmt, va_list ap)
{
	if (level >= ORTP_WARNING) {
		gchar *message = g_strdup_vprintf(fmt, ap);
		g_debug("voip-linphone: * %s", message);
		g_free(message);
	}
}

static gboolean voip_timeout(gpointer user_data)
{
	struct voip *voip = user_data;

	linphone_core_iterate(voip->core);

	return TRUE;
}

static void voip_codec_enable(struct voip *voip, const char *mime_type,
			      gboolean enable)
{
	const MSList *codecs;
	const MSList *node;

	codecs = linphone_core_get_audio_codecs(voip->core);

	for (node = codecs; node; node = ms_list_next(node)) {
		PayloadType *pt = node->data;

		if (g_ascii_strcasecmp(pt->mime_type, mime_type) != 0)
			continue;

		linphone_core_enable_payload_type(voip->core, pt, enable);
		g_debug("voip-linphone: %s payload type: %s, %d Hz",
			enable ? "enabling" : "disabling",
			pt->mime_type, pt->clock_rate);
	}
}

int voip_create(struct voip **voipp, struct rpc_server *server,
		GKeyFile *config)
{
	const char *factory_config = SYSCONF_DIR "/linphone.conf";
	struct remote_control *rc = rpc_server_priv(server);
	struct voip *voip;
	gchar **codecs;
	gboolean ec;

	if (!voipp)
		return -EINVAL;

	voip = g_new0(struct voip, 1);
	if (!voip)
		return -ENOMEM;

	voip->timeout = g_timeout_source_new(50);
	if (!voip->timeout) {
		g_free(voip);
		return -ENOMEM;
	}

	g_source_set_callback(voip->timeout, voip_timeout, voip, NULL);

	linphone_core_enable_logs_with_cb(linphone_log);

	voip->core = linphone_core_new(&vtable, NULL, factory_config, rc);
	if (!voip->core) {
		g_source_unref(voip->timeout);
		g_free(voip);
		return -ENOMEM;
	}

	codecs = g_key_file_get_string_list(config, "linphone",
			"disable-codecs", NULL, NULL);
	if (codecs) {
		gchar **codec;

		for (codec = codecs; *codec; codec++)
			voip_codec_enable(voip, *codec, false);

		g_strfreev(codecs);
	}
	codecs = g_key_file_get_string_list(config, "linphone",
			"enable-codecs", NULL, NULL);
	if (codecs) {
		gchar **codec;

		for (codec = codecs; *codec; codec++)
			voip_codec_enable(voip, *codec, true);

		g_strfreev(codecs);
	}

	/*
	 * We don't bother checking for errors here because by default we want
	 * to turn echo cancellation off, which is the value returned by the
	 * g_key_file_get_boolean() function if the key either was not found
	 * or cannot be interpreted as a boolean.
	 */
	ec = g_key_file_get_boolean(config, "linphone", "echo-cancellation", NULL);
	g_debug("voip-linphone: echo cancellation %sabled", ec ? "en" : "dis");
	linphone_core_enable_echo_cancellation(voip->core, ec);

	/*
	 * Allow the default expiration time for SIP registrations to be
	 * overridden.
	 */
	if (g_key_file_has_key(config, "linphone", "registration-expiry", NULL)) {
		voip->expires = g_key_file_get_integer(config, "linphone",
						       "registration-expiry",
						       NULL);
		if (voip->expires != 0)
			voip->expires = CLAMP(voip->expires, 30, 3600);
	}

	voip->contact_display = voip->contact_name = NULL;

	/*
	 * FIXME: This code was initially meant to set the output volume to
	 * 100% but it doesn't. It actually sets the output volume gain,
	 * meaning that the volume is really amplified by about 26%. Alas,
	 * some devices have already shipped with this code, so we'll
	 * probably have to stick with it forever.
	 */
	linphone_core_set_playback_gain_db(voip->core, 1.0f);
	linphone_core_set_ring(voip->core, NULL);

	/*
	 * The keepi-alive period can be set in .linphonerc or /etc/linphone.conf.
	 * But the API does not allow to set the period directly, so we can only
	 * enable/disable keep-alive.
	 */
	if (g_key_file_has_key(config, "linphone", "keep-alive", NULL)) {
		gboolean enable = g_key_file_get_boolean(config, "linphone",
		                                         "keep-alive", NULL);
		linphone_core_enable_keep_alive(voip->core, enable);
	}

	if (g_key_file_has_key(config, "linphone", "soundcard", NULL)) {
		gchar *card = g_key_file_get_string(config, "linphone",
						    "soundcard", NULL);
		voip_set_playback(voip, card);
		voip_set_capture(voip, card);
		g_free(card);
	}

	/*
	 * FIXME: This should be removed, the configuration should be done
	 *        via linphone's own config file.
	 */
	if (g_key_file_has_key(config, "linphone", "qos-dscp", NULL)) {
		int dscp = g_key_file_get_integer(config, "linphone",
						  "qos-dscp", NULL);
		linphone_core_set_audio_dscp(voip->core, dscp);
	}

	*voipp = voip;
	return 0;
}

int voip_free(struct voip *voip)
{
	if (!voip)
		return -EINVAL;

	linphone_core_terminate_all_calls(voip->core);
	voip_logout(voip);
	linphone_core_destroy(voip->core);
	g_free(voip->contact_name);
	g_free(voip->contact_display);
	g_free(voip);

	return 0;
}

GSource *voip_get_source(struct voip *voip)
{
	return voip ? voip->timeout : NULL;
}

static int is_valid_string(const char* str)
{
	if (!str)
		return FALSE;

	return strlen(str) > 0;
}

int voip_login(struct voip *voip, const char *host, uint16_t port,
	       const char *username, const char *password,
	       enum voip_transport transport)
{
	LinphoneProxyConfig *proxy;
	LCSipTransports transports;
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

	memset(&transports, 0, sizeof(transports));

	switch (transport) {
	case VOIP_TRANSPORT_UDP:
		transports.udp_port = port;
		break;

	case VOIP_TRANSPORT_TCP:
		transports.tcp_port = port;
		break;

	case VOIP_TRANSPORT_TLS:
		transports.tls_port = port;
		break;

	default:
		return -EINVAL;
	}

	if (linphone_core_set_sip_transports(voip->core, &transports) < 0)
		g_debug("voip-linphone: failed to set transports");

	if (!port || !is_valid_string(host) || !is_valid_string(username) ||
	    !is_valid_string(password))
	{
		linphone_core_get_default_proxy(voip->core, &proxy);
		if (proxy) {
			if (!linphone_proxy_config_is_registered(proxy)) {
				linphone_proxy_config_edit(proxy);
				linphone_proxy_config_enable_register(proxy,
						TRUE);
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
	if (!linphone_address_is_sip(address)) {
		g_warning("voip-linphone: failed to create linphone address "
			"using '%s'", server);
		free(server);
		free(user);
		return -EINVAL;
	}

	linphone_address_set_display_name(address, user);
	linphone_address_set_username(address, user);
	linphone_address_set_domain(address, domain);
	linphone_address_set_port(address, port);
	identity = linphone_address_as_string(address);
	linphone_address_destroy(address);

	auth = linphone_auth_info_new(user, NULL, password, NULL, NULL, NULL);
	linphone_core_add_auth_info(voip->core, auth);

	linphone_proxy_config_set_server_addr(proxy, server);
	linphone_proxy_config_set_identity(proxy, identity);

	if (voip->expires) {
		g_debug("voip-linphone: registration expires in %d seconds",
			voip->expires);
		linphone_proxy_config_expires(proxy, voip->expires);
	}

	linphone_proxy_config_enable_register(proxy, TRUE);

	if (!use_default) {
		linphone_core_add_proxy_config(voip->core, proxy);
		linphone_core_set_default_proxy(voip->core, proxy);
	} else {
		int err = linphone_proxy_config_done(proxy);
		if (err < 0)
			g_debug("voip-linphone: proxy configuration failed");
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
		if (err < 0)
			g_debug("voip-linphone: proxy configuration failed");
	}

	linphone_core_clear_proxy_config(voip->core);
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

	if (caller) {
		if (voip->contact_display)
			*caller = g_strdup(voip->contact_display);
		else if (voip->contact_name)
			*caller = g_strdup(voip->contact_name);
	}

	return 0;
}

int voip_terminate(struct voip *voip)
{
	int err;

	if (!voip)
		return -EINVAL;

	/*
	 * We only support one call at a time, so we can always
	 * terminate all calls.
	 */
	err = linphone_core_terminate_all_calls(voip->core);
	if (err < 0)
		return err;

	return 0;
}

int voip_get_login_state(struct voip *voip, enum voip_login_state *statep)
{
	enum voip_login_state state = VOIP_LOGIN_STATE_LOGGED_OUT;
	LinphoneProxyConfig *proxy = NULL;

	if (!voip || !statep)
		return -EINVAL;

	linphone_core_get_default_proxy(voip->core, &proxy);
	if (proxy && linphone_proxy_config_is_registered(proxy))
		state = VOIP_LOGIN_STATE_LOGGED_IN;

	*statep = state;
	return 0;
}

int voip_get_contact(struct voip *voip, const char **namep, const char **displayp)
{
	if (!voip)
		return -EINVAL;

	if (namep)
		*namep = voip->contact_name;
	if (displayp)
		*displayp = voip->contact_display;
	return 0;
}

int voip_dial(struct voip *voip, uint8_t dtmf)
{
	if (!voip)
		return -EINVAL;

	/* we can only send dtmf tone on running calls */
	if (!linphone_core_in_call(voip->core))
		return -ENOTCONN;

	linphone_core_send_dtmf(voip->core, (char)dtmf);
	return 0;
}

int voip_set_playback(struct voip *voip, const char *card_name)
{
	if (!voip)
		return -EINVAL;

	int err = linphone_core_set_playback_device(voip->core, card_name);
	if (err < 0)
		g_warning("voip-linphone: failed to set playback device");
	else
		g_debug("voip-linphone: set playback device to: %s", card_name);
	return err;
}

int voip_set_capture(struct voip *voip, const char *card_name)
{
	if (!voip)
		return -EINVAL;

	int err = linphone_core_set_capture_device(voip->core, card_name);
	if (err < 0)
		g_warning("voip-linphone: failed to set capture device");
	else
		g_debug("voip-linphone: set capture device to: %s", card_name);
	return err;
}

int voip_set_capture_gain(struct voip *voip, float gain)
{
	struct _LpConfig *conf;

	if (!voip)
		return -EINVAL;

	conf = linphone_core_get_config(voip->core);
	if (!conf) {
		g_warning("voip-linphone: failed to get config while setting capture gain");
		return -EINVAL;
	}

	lp_config_set_float(conf, "sound", "mic_gain", gain);
	return 0;
}

int voip_set_onstatechange_cb(struct voip *voip, voip_onstatechange_cb cb,
			      void *cb_data, void *owner_ref)
{
	if (!voip)
		return -EINVAL;

	voip->onstatechange_cb = cb;
	voip->callback_data = cb_data;
	voip->callback_owner = owner_ref;

	return 0;
}

void *voip_get_onstatechange_cb_owner(struct voip *voip)
{
	return voip ? voip->callback_owner : NULL;
}
