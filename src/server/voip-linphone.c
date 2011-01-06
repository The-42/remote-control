#include <linphonecore.h>
#include <glib.h>

#include "remote-control-stub.h"
#include "remote-control.h"

struct voip {
	LinphoneCore *core;
	char *contact;

	pthread_t thread;
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

static void linphone_call_state_changed_cb(LinphoneCore *core, LinphoneCall *call, LinphoneCallState state, const char *message)
{
	struct remote_control *rc = linphone_core_get_user_data(core);
	const LinphoneAddress *address;
	struct voip *voip = rc->voip;
	struct event event;

	g_print("LINPHONE: CALL STATE: %p: %s", call, linphone_call_state_to_string(state));

	if (message)
		g_print(" (%s)", message);

	g_print("\n");

	memset(&event, 0, sizeof(event));
	event.source = EVENT_SOURCE_VOIP;

	switch (state) {
	case LinphoneCallIncomingReceived:
		event.voip.state = EVENT_VOIP_STATE_INCOMING;
		event_manager_report(rc->event_manager, &event);

		if (voip->contact) {
			ms_free(voip->contact);
			voip->contact = NULL;
		}

		address = linphone_call_get_remote_address(call);
		if (address) {
			const char *name = linphone_address_get_display_name(address);
			if (name)
				voip->contact = strdup(name);
		}
		break;

	case LinphoneCallEnd:
		event.voip.state = EVENT_VOIP_STATE_INCOMING_DISCONNECTED;
		event_manager_report(rc->event_manager, &event);
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

static void *voip_thread(void *context)
{
	struct voip *voip = context;

	while (!voip->done) {
		linphone_core_iterate(voip->core);
		usleep(50000);
	}

	return NULL;
}

int voip_create(struct voip **voipp, struct rpc_server *server)
{
	struct remote_control *rc = rpc_server_priv(server);
	struct voip *voip;
	int err;

	if (!voipp)
		return -EINVAL;

	voip = malloc(sizeof(*voip));
	if (!voip)
		return -ENOMEM;

	memset(voip, 0, sizeof(*voip));

	voip->core = linphone_core_new(&vtable, NULL, NULL, rc);
	if (!voip->core) {
		free(voip);
		return -ENOMEM;
	}

	err = pthread_create(&voip->thread, NULL, voip_thread, voip);
	if (err != 0) {
		g_error("failed to create thread: %d", err);
		return err;
	}

	*voipp = voip;
	return 0;
}

int voip_free(struct voip *voip)
{
	if (!voip)
		return -EINVAL;

	voip->done = true;
	pthread_join(voip->thread, NULL);

	ms_free(voip->contact);
	linphone_core_destroy(voip->core);
	free(voip);
	return 0;
}

int voip_login(struct voip *voip, const char *host, uint16_t port,
		const char *username, const char *password)
{
	LinphoneProxyConfig *proxy;
	LinphoneAddress *address;
	bool use_default = true;
	LinphoneAuthInfo *auth;
	char *server = NULL;
	char *identity;
	int len;

	g_debug("> %s(voip=%p, host=%s, port=%u, username=%s, password=%s)",
			__func__, voip, host, port, username, password);

	if (!voip)
		return -EINVAL;

	if (!host || !port || !username || !password) {
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

	address = linphone_address_new(server);
	linphone_address_set_display_name(address, username);
	linphone_address_set_username(address, username);
	linphone_address_set_domain(address, host);
	linphone_address_set_port_int(address, port);
	identity = linphone_address_as_string(address);
	linphone_address_destroy(address);

	auth = linphone_auth_info_new(username, NULL, password, NULL, NULL);
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
	LinphoneCall *call;

	if (!voip)
		return -EINVAL;

	call = linphone_core_invite(voip->core, uri);
	if (!call)
		return -EIO;

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
		*caller = strdup(voip->contact);

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
