/**
 * @file
 * @author Avionic Design GmbH
 * @date 2017
 * @copyright This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 3 or
 * later as published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <errno.h>
#include <glib.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "javascript.h"

/**
 * @defgroup MEDIAL_MESSAGES Medial message structures
 *
 * Medial messages; inspired by project 1363, padded & salted with bits and
 * pieces supplied by (scarce) documentation, loose chipcards and Wireshark.
 *
 * @remark Many message parts use <tt>The Medial Wicked Format (tm)</tt> or
 * stringified hex.
 * Stringified hex is a rather obscure format indicating a packet's payload's
 * length or various other numeric values. The number is converted to hex, taken
 * apart, and each half-byte gets the offset of an ASCII '0' applied, 0x30h.
 * This results in things like '30 3D' (ASCII "0=", actually 0x0D or 13), which
 * sort of isn't always readable.
 *
 * @{
 */

#define MAX_MSG_LEN	128	/**< Max. message size, both ways */
#define MIN_MSG_LEN	27	/**< Min. message size, from Medial */

/**
 * @brief Medial message header
 *
 * Each message starts with this header used for indicating the location of the
 * sender, message type, payload length and optionally its SIP address.
 */
struct medial_header {
	uint8_t stx;		/**< Start byte (fixed 02h)*/
	uint16_t ref;		/**< Reference number */
	char mac[12];		/**< MAC address, or some other unique ID */
	char ipaddr[8];		/**< SIP address */
	uint16_t type;		/**< Message type */
	uint16_t length;	/**< Length of bytes follow */
} __attribute__((packed));

/**
 * @brief Medial "powerup" message
 *
 * Message used for registering the device with the Medial server. It includes
 * smartcard data if available, dummy data as replacement for the latter
 * otherwise. This message shall always be sent on first contact with the Medial
 * server after a reset of any type and before sending "card_change" messages.
 *
 * @note Can lead to a response with error indication, even though the data
 * might be valid. The consequences appear to be nonexisting.
 */
struct medial_powerup {
	/** Message header, @sa medial_header */
	struct medial_header header;
	uint8_t uid;		/**< Unit ID */
	uint8_t ioport[8];	/**< I/O Port (unused) */
	char acl[8];		/**< ACL word */
	uint8_t state;		/**< Card State */
	char cid[3];		/**< Card ID */
	uint8_t mid;		/**< Client number */
	char sname[8];		/**< System name */
	char cardno[7];		/**< Card number */
	uint8_t etx;		/**< End byte (fixed 03h)*/
} __attribute__((packed));

/**
 * @brief Medial "card_change" message
 *
 * Message for indicating a smartcard event. Insertion and removal both use data
 * from the smartcard (which thus must be stored separately), the only
 * difference between login and logout, when referring to the same card, is the
 * @c state byte.
 */
struct medial_card_change {
	/** Message header, @sa medial_header */
	struct medial_header header;
	uint8_t state;		/**< Card inserted state */
	uint8_t type;		/**< Card type (for Medcom 60h) */
	char cid[3];		/**< Medcom card subtype (for Medcom "101") */
	uint8_t mid;		/**< Client number*/
	char sname[8];		/**< System name */
	char cardno[7];		/**< Card number */
	uint8_t etx;		/**< End byte (fixed 03h) */
} __attribute__((packed));

/**
 * @brief Medial "response" message
 *
 * Status message sent from the Medial server to the device. Contains either
 * acknowledge, disapproval or a I-don't-know-who-you-are response to the last
 * message from the device.
 */
struct medial_response {
	/** Message header, @sa medial_header */
	struct medial_header header;
	uint8_t status;		/**< Status byte */
	uint8_t etx;		/**< End byte */
} __attribute__((packed));

/**
 * @brief Medial "apply" message.
 *
 * This message is often sent after a regular "response" message to indicate the
 * current balance state for the given account. At the moment, this appears to
 * be the only purpose for this packet. While capable for more, most of the
 * bytes remain unused. If no other messages are sent, this message is repeated
 * thrice in intervals of roughly 10 seconds.
 *
 * The portstate encodes the balance state in bits 8..11, which is either red,
 * yellow or green and has an additional bit indicating whether the account has
 * been closed. Order from MSB (bit 11) to LSB (bit 8): "closed", "green",
 * "yellow", "red". Bits are inversed in their meaning. A status byte of
 * @c nnnnnBnnh thus indicates an open account with green balance, @c nnnnn6nnh
 * red and closed.
 *
 * @note The balance status currently appears to be broken as it returns @c Eh
 * (open, red) for any card, regardless of its real state (which is reported
 * correctly via the SOAP interface). Do not rely on the status.
 */
struct medial_apply {
	struct medial_header header;
	uint8_t  portmode;	/**< Port mode byte (unused) */
	uint32_t portmask;	/**< Port mask dword (unused) */
	uint32_t portstate;	/**< Port state (unused except bits 8..11) */
	uint32_t time;		/**< Timestamp (unused) */
	uint8_t etx;		/**< End byte (fixed 02h) */
} __attribute__((packed));

/**
 * @brief Medial message types
 *
 * The Medial server knowns four types of messages it sends to the client. For
 * specifics about which may be sent when, see descriptions for medial_response,
 * medial_powerup, medial_card_change and medial_apply.
 */
enum medial_message_type {
	MEDIAL_MSG_RESP = 0x50,	/**< Medial "response" message */
	MEDIAL_MSG_POWERUP,	/**< Medial "powerup" message */
	MEDIAL_MSG_CARD = 0x53,	/**< Medial "card_change" message */
	MEDIAL_MSG_APPLY	/**< Medial "apply" message */
};

/**
 * @brief Medial "response" status values
 *
 * The Medial server responds with a tristate value: "It worked",
 * "it didn't work" and "I don't know you". When getting a response containing
 * the last value, the server expects it configuration URI used for localisation
 * to be called in order to add the location to its database.
 */
enum medial_status_byte {
	MEDIAL_OK = 0x30,	/**< Last action is confirmed */
	MEDIAL_ERROR,		/**< Last action is denied */
	MEDIAL_LOCATION		/**< Last action refers to unknown location */
};

/** @} */

struct medial_parameters {
	char location_id[13];
	char system_name[9];

	unsigned int card_number;
	uint8_t card_state;
};

#define MEDIAL_ACL	"AVAYA007"
#define CARD_SUBTYPE	"101"
#define CLIENT_NUMBER	'1'

struct js_medial {
	GSource source;
	struct medial_parameters *medial;
	struct net_udp *net_udp;
	int udp_channel;
	JSContextRef context;
	JSObjectRef cb_response;
	JSObjectRef cb_apply;
	JSObjectRef this;
	GList *events;
};


static void mmsg_init_header(struct medial_header* header, char *mac,
		char *phone)
{
	char buffer[13];

	if (!header)
		return;

	header->stx = 0x02;
	header->ref = '0' << 8 | '0'; /* ASCII... */
	snprintf(buffer, 13, "%12s", mac);
	strncpy(header->mac, buffer, 12);
	snprintf(buffer, 9, "%8s", phone);
	strncpy(header->ipaddr, buffer, 8);
}

static void mmsg_send_header_preproc(struct medial_header* header)
{
	if (!header)
		return;

	header->ref = htons(header->ref);
	header->type = htons(header->type);
	header->length = htons(header->length);
}

static void mmsg_recv_header_postproc(struct medial_header* header)
{
	if (!header)
		return;

	header->ref = ntohs(header->ref);
	header->type = ntohs(header->type);
	header->length = ntohs(header->length);
}

static void mmsg_recv_apply_postproc(struct medial_apply* appl)
{
	if (!appl)
		return;

	appl->portmask = ntohl(appl->portmask);
	appl->portstate = ntohl(appl->portstate);
	appl->time = ntohl(appl->time);
}

static void mmsg_init_powerup_message(struct medial_parameters *ctx,
		struct medial_powerup *pwr)
{
	char buffer[9];

	if (!ctx || !pwr)
		return;

	mmsg_init_header(&pwr->header, ctx->location_id, ctx->system_name);

	pwr->header.type = '5' << 8 | '1';	/* 51h -> PDU 81 */
	pwr->header.length = '2' << 8 | '5';	/* 25h -> 37 + etx */


	pwr->uid = '1';
	memset(pwr->ioport, ' ', 8);
	strncpy(pwr->acl, MEDIAL_ACL, 8);

	pwr->state = ctx->card_state > 0 ? '1' : '0';
	strncpy(pwr->cid, CARD_SUBTYPE, strlen(CARD_SUBTYPE));
	pwr->mid = CLIENT_NUMBER;
	snprintf(buffer, 9, "%8s", ctx->system_name);
	strncpy(pwr->sname, buffer, 8);
	snprintf(buffer, 8, "%07d", ctx->card_number);
	strncpy(pwr->cardno, buffer, 7);
	pwr->etx = 0x03;
}

static void mmsg_init_card_message(struct medial_parameters *ctx,
		struct medial_card_change *card)
{
	char buffer[9];

	if (!ctx || !card)
		return;

	mmsg_init_header(&card->header, ctx->location_id, ctx->system_name);

	card->header.type = '5' << 8 | '3';	/* 53h -> PDU 83 */
	card->header.length = '1' << 8 | '5';	/* 15h -> 21 + etx */

	card->state = ctx->card_state > 0 ? '1' : '0';
	card->type = 0x60;
	strncpy(card->cid, "101", 3);
	card->mid = '1';
	snprintf(buffer, 9, "%8s", ctx->system_name);
	strncpy(card->sname, buffer, 8);
	snprintf(buffer, 8, "%07d", ctx->card_number);
	strncpy(card->cardno, buffer, 7);
	card->etx = 0x03;
}

static int mmsg_get_type(uint8_t *data, size_t len)
{
	if (!data || len < 25)
		return -EINVAL;

	return (data[23] - 0x30) << 4 | (data[24] - 0x30);
}

static JSValueRef jsstring_from_response(JSContextRef js, uint8_t byte,
		JSValueRef *exception)
{
	switch (byte) {
	case MEDIAL_OK:
		return javascript_make_string(js, "ok", exception);

	case MEDIAL_ERROR:
		return javascript_make_string(js, "error", exception);

	case MEDIAL_LOCATION:
		return javascript_make_string(js, "no location", exception);

	default:
		g_warning("medial msg: unknown response code: 0x%02X", byte);
		return javascript_make_string(js, "unknown", exception);
	}

	return JSValueMakeNull(js);
}

static void js_medial_digest_packet(struct js_medial *jsdg,
		struct net_udp_channel *channel)
{
	struct net_udp_channel *js_chan;
	struct medial_apply *m_apply;
	JSValueRef exception = NULL;
	JSValueRef args[2];
	char bal_tuple[4];
	uint8_t balance;
	uint8_t *data;
	int size;
	int type;

	js_chan = net_udp_get_channel_by_ref(jsdg->net_udp, jsdg->udp_channel);
	if (js_chan != channel) {
		g_warning("%s: channel mismatch, discarding packet", __func__);
		return;
	}

	data = g_malloc(MAX_MSG_LEN);
	size = net_udp_recv(channel, data, MAX_MSG_LEN);
	if (size < 0) {
		g_warning("%s: failed to read packet: %d", __func__, size);
		g_free(data);
		return;
	}

	if (size < MIN_MSG_LEN) {
		g_warning("%s: impossible message length: %d", __func__, size);
		g_free(data);
		return;
	}

	type = mmsg_get_type(data, size);
	if (type < 0) {
		g_warning("%s: invalid message", __func__);
		g_free(data);
		return;
	}

	mmsg_recv_header_postproc((struct medial_header *)data);

	switch (type) {
	case MEDIAL_MSG_RESP:
		if (!jsdg->cb_response)
			break;

		args[0] = jsstring_from_response(jsdg->context,
			((struct medial_response *)data)->status, &exception);
		if (exception) {
			g_warning(JS_LOG_CALLBACK_EXCEPTION, __func__);
			break;
		}

		(void)JSObjectCallAsFunction(jsdg->context, jsdg->cb_response,
			jsdg->this, 1, args, &exception);
		if (exception)
			g_warning(JS_LOG_CALLBACK_EXCEPTION, __func__);
		break;

	case MEDIAL_MSG_APPLY:
		if (!jsdg->cb_apply)
			break;

		m_apply = (struct medial_apply *)data;
		mmsg_recv_apply_postproc(m_apply);
		balance = (m_apply->portstate >> 8) & 0x0F;
		args[0] = javascript_make_string(jsdg->context,
			balance & 0x08 ? "open" : "closed", &exception);
		if (exception) {
			g_warning(JS_LOG_CALLBACK_EXCEPTION, __func__);
			break;
		}

		snprintf(bal_tuple, sizeof(bal_tuple), "%c%c%c",
			balance & 0x04 ? ' ' : 'g',
			balance & 0x02 ? ' ' : 'y',
			balance & 0x01 ? ' ' : 'r');
		args[1] = javascript_make_string(jsdg->context, bal_tuple,
			&exception);
		if (exception) {
			g_warning(JS_LOG_CALLBACK_EXCEPTION, __func__);
			break;
		}

		(void)JSObjectCallAsFunction(jsdg->context, jsdg->cb_apply,
			jsdg->this, G_N_ELEMENTS(args), args, &exception);
		if (exception)
			g_warning(JS_LOG_CALLBACK_EXCEPTION, __func__);
		break;

	case MEDIAL_MSG_POWERUP:
		g_warning("%s: Medial 'powerup' message received from server?!",
			__func__);
		break;

	case MEDIAL_MSG_CARD:
		g_warning("%s: Medial 'card' message received from server?!",
			__func__);
		break;

	default:
		g_warning("%s: unknown message type: 0x%02X", __func__, type);
	}

	g_free(data);
}

static void js_packet_received_cb(struct net_udp_channel *channel, void *data)
{
	struct js_medial *jsdg = data;
	struct net_udp_channel **evt;

	if (!jsdg)
		return;

	evt = g_new0(struct net_udp_channel*, 1);
	if (!evt)
		return;

	*evt = channel;
	jsdg->events = g_list_append(jsdg->events, evt);
}

static gboolean js_medial_source_prepare(GSource *source, gint *timeout)
{
	if (timeout)
		*timeout = 100;

	return FALSE;
}

static gboolean js_medial_source_check(GSource *source)
{
	struct js_medial *jsdg = (struct js_medial *)source;

	if (jsdg && g_list_first(jsdg->events))
		return TRUE;

	return FALSE;
}
static gboolean js_medial_source_dispatch(GSource *source,
		GSourceFunc callback, gpointer user_data)
{
	struct js_medial *jsdg = (struct js_medial *)source;
	GList *node = jsdg ? g_list_first(jsdg->events) : NULL;
	struct net_udp_channel **evt = node ? node->data : NULL;

	while (evt) {
		if (jsdg->context)
			js_medial_digest_packet(jsdg, *evt);
		jsdg->events = g_list_remove(jsdg->events, evt);
		g_free(evt);
		node = g_list_first(jsdg->events);
		evt = node ? node->data : NULL;
	}

	if (callback)
		return callback(user_data);

	return TRUE;
}

static void js_medial_source_finalize(GSource *source)
{
	struct js_medial *jsdg = (struct js_medial *)source;

	if (jsdg)
		g_list_free_full(jsdg->events, g_free);
}

static GSourceFuncs js_medial_source_funcs = {
	.prepare = js_medial_source_prepare,
	.check = js_medial_source_check,
	.dispatch = js_medial_source_dispatch,
	.finalize = js_medial_source_finalize,
};

static JSValueRef js_medial_get_on_response_msg(JSContextRef context,
		JSObjectRef object, JSStringRef name, JSValueRef *exception)
{
	struct js_medial *jsdg = JSObjectGetPrivate(object);

	if (!jsdg) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	return jsdg->cb_response ? JSValueMakeNull(context) : jsdg->cb_response;
}

static bool js_medial_set_on_response_msg(JSContextRef context,
		JSObjectRef object, JSStringRef name, JSValueRef value,
		JSValueRef *exception)
{
	struct js_medial *jsdg = JSObjectGetPrivate(object);

	if (!jsdg) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return false;
	}

	if (jsdg->cb_response)
		JSValueUnprotect(jsdg->context, jsdg->cb_response);

	if (JSValueIsNull(context, value)) {
		jsdg->cb_response = NULL;
		return true;
	}

	jsdg->cb_response = JSValueToObject(context, value, exception);
	if (!jsdg->cb_response) {
		javascript_set_exception_text(context, exception,
			"failed to assign JS callback");
		return false;
	}
	JSValueProtect(context, jsdg->cb_response);

	return true;
}

static JSValueRef js_medial_get_on_apply_msg(JSContextRef context,
		JSObjectRef object, JSStringRef name, JSValueRef *exception)
{
	struct js_medial *jsdg = JSObjectGetPrivate(object);

	if (!jsdg) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	return jsdg->cb_apply ? JSValueMakeNull(context) : jsdg->cb_apply;
}

static bool js_medial_set_on_apply_msg(JSContextRef context,
		JSObjectRef object, JSStringRef name, JSValueRef value,
		JSValueRef *exception)
{
	struct js_medial *jsdg = JSObjectGetPrivate(object);

	if (!jsdg) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return false;
	}

	if (jsdg->cb_apply)
		JSValueUnprotect(jsdg->context, jsdg->cb_apply);

	if (JSValueIsNull(context, value)) {
		jsdg->cb_apply = NULL;
		return true;
	}

	jsdg->cb_apply = JSValueToObject(context, value, exception);
	if (!jsdg->cb_apply) {
		javascript_set_exception_text(context, exception,
			"failed to assign JS callback");
		return false;
	}
	JSValueProtect(context, jsdg->cb_apply);

	return true;
}

static const JSStaticValue medial_properties[] = {
	{
		.name = "onResponseMessage",
		.getProperty = js_medial_get_on_response_msg,
		.setProperty = js_medial_set_on_response_msg,
		.attributes = kJSPropertyAttributeNone,
	},
	{
		.name = "onApplyMessage",
		.getProperty = js_medial_get_on_apply_msg,
		.setProperty = js_medial_set_on_apply_msg,
		.attributes = kJSPropertyAttributeNone,
	},
	{}
};

static void medial_reset_channel(struct js_medial *jsdg)
{
	if (jsdg && jsdg->udp_channel > -1) {
		net_udp_destroy_channel(jsdg->net_udp, jsdg->udp_channel);
		jsdg->udp_channel = -1;
	}
}

static JSValueRef medial_function_init(JSContextRef js,
		JSObjectRef function, JSObjectRef object, size_t argc,
		const JSValueRef argv[], JSValueRef *exception)
{
	struct js_medial *jsdg = JSObjectGetPrivate(object);
	struct net_udp_channel *channel;
	int port_remote;
	int port_local;
	char *hostname;
	char *sysname;
	int ret;

	if (!jsdg) {
		javascript_set_exception_text(js, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	if (jsdg->medial)
		g_free(jsdg->medial);

	if (argc != 4) {
		javascript_set_exception_text(js, exception,
			"invalid arguments count: "
			"use 'hostname, remote_port, local_port, medial_name'");
		return NULL;
	}

	ret = javascript_int_from_number(js, argv[1], 0, UINT16_MAX,
		&port_remote, exception);
	if (ret)
		return NULL;


	ret = javascript_int_from_number(js, argv[2], 0, UINT16_MAX,
		&port_local, exception);
	if (ret)
		return NULL;

	hostname = javascript_get_string(js, argv[0], exception);
	if (!hostname)
		return NULL;

	sysname = javascript_get_string(js, argv[3], exception);
	if (!sysname)
		return NULL;

	medial_reset_channel(jsdg);

	ret = net_udp_create_channel(jsdg->net_udp, port_local, hostname,
		port_remote);
	if (ret < 0) {
		javascript_set_exception_text(js, exception,
			"Creating Medial UDP channel failed: %d", ret);
		return NULL;
	}

	jsdg->udp_channel = ret;

	channel = net_udp_get_channel_by_ref(jsdg->net_udp, jsdg->udp_channel);
	ret = net_udp_set_recv_cb(channel, js_packet_received_cb, jsdg);
	if (ret) {
		javascript_set_exception_text(js, exception,
			"Creating Medial UDP callback failed: %d", ret);
		return NULL;
	}

	jsdg->medial = g_new0(struct medial_parameters, 1);
	snprintf(jsdg->medial->system_name, sizeof(jsdg->medial->system_name),
		"%s", sysname);

	g_free(hostname);
	g_free(sysname);

	return JSValueMakeNull(js);
}

static JSValueRef medial_function_register(JSContextRef js,
		JSObjectRef function, JSObjectRef object, size_t argc,
		const JSValueRef argv[], JSValueRef *exception)
{
	struct js_medial *jsdg = JSObjectGetPrivate(object);
	struct net_udp_channel *channel;
	struct medial_powerup data;
	char *location_id;
	int ret;

	if (!jsdg) {
		javascript_set_exception_text(js, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	if (argc != 1) {
		javascript_set_exception_text(js, exception,
			"invalid argument count: use 'location_id'");
		return NULL;
	}

	if (!jsdg->medial) {
		javascript_set_exception_text(js, exception,
			"medial object not initialized, use init() first");
		return NULL;
	}

	location_id = javascript_get_string(js, argv[0], exception);
	if (!location_id)
		return NULL;

	snprintf(jsdg->medial->location_id, sizeof(jsdg->medial->location_id),
		"%s", location_id);
	g_free(location_id);

	channel = net_udp_get_channel_by_ref(jsdg->net_udp, jsdg->udp_channel);
	if (!channel) {
		javascript_set_exception_text(js, exception,
			"no channel for this reference");
		return NULL;
	}

	mmsg_init_powerup_message(jsdg->medial, &data);
	mmsg_send_header_preproc(&data.header);

	ret = net_udp_send(channel, (uint8_t *)&data, sizeof(data));
	if (ret < 0) {
		javascript_set_exception_text(js, exception,
			"failed to send 'powerup' message: %d", ret);
	}

	return ret >= 0 ? JSValueMakeNull(js) : NULL;
}

static JSValueRef medial_function_login(JSContextRef js,
		JSObjectRef function, JSObjectRef object, size_t argc,
		const JSValueRef argv[], JSValueRef *exception)
{
	struct js_medial *jsdg = JSObjectGetPrivate(object);
	struct net_udp_channel *channel;
	struct medial_card_change data;
	int card_id;
	int ret;

	if (!jsdg) {
		javascript_set_exception_text(js, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	if (argc != 1) {
		javascript_set_exception_text(js, exception,
			"invalid argument count: use 'card_id'");
		return NULL;
	}

	if (!jsdg->medial) {
		javascript_set_exception_text(js, exception,
			"medial object not initialized, use init() first");
		return NULL;
	}

	ret = javascript_int_from_number(js, argv[1], 0, UINT16_MAX,
		&card_id, exception);
	if (ret)
		return NULL;

	jsdg->medial->card_number = card_id;
	jsdg->medial->card_state = 1;

	channel = net_udp_get_channel_by_ref(jsdg->net_udp, jsdg->udp_channel);
	if (!channel) {
		javascript_set_exception_text(js, exception,
			"no channel for this reference");
		return NULL;
	}

	mmsg_init_card_message(jsdg->medial, &data);
	mmsg_send_header_preproc(&data.header);

	ret = net_udp_send(channel, (uint8_t *)&data, sizeof(data));
	if (ret < 0) {
		javascript_set_exception_text(js, exception,
			"failed to send 'card' message: %d", ret);
	}

	return ret >= 0 ? JSValueMakeNull(js) : NULL;
}

static JSValueRef medial_function_logout(JSContextRef js,
		JSObjectRef function, JSObjectRef object, size_t argc,
		const JSValueRef argv[], JSValueRef *exception)
{
	struct js_medial *jsdg = JSObjectGetPrivate(object);
	struct net_udp_channel *channel;
	struct medial_card_change data;
	int card_id;
	int ret;

	if (!jsdg) {
		javascript_set_exception_text(js, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return NULL;
	}

	if (argc > 1) {
		javascript_set_exception_text(js, exception,
			"invalid argument count: use '[card_id]'");
		return NULL;
	}

	if (!jsdg->medial) {
		javascript_set_exception_text(js, exception,
			"medial object not initialized, use init() first");
		return NULL;
	}

	if (argc == 1) {
		ret = javascript_int_from_number(js, argv[0], 0, UINT16_MAX,
			&card_id, exception);
		if (ret)
			return NULL;

		jsdg->medial->card_number = card_id;
	}
	jsdg->medial->card_state = 0;

	channel = net_udp_get_channel_by_ref(jsdg->net_udp, jsdg->udp_channel);
	if (!channel) {
		javascript_set_exception_text(js, exception,
			"no channel for this reference");
		return NULL;
	}

	mmsg_init_card_message(jsdg->medial, &data);
	mmsg_send_header_preproc(&data.header);

	ret = net_udp_send(channel, (uint8_t *)&data, sizeof(data));
	if (ret < 0) {
		javascript_set_exception_text(js, exception,
			"failed to send 'card' message: %d", ret);
	}

	return ret >= 0 ? JSValueMakeNull(js) : NULL;
}

static const JSStaticFunction medial_functions[] = {
	{
		.name = "init",
		.callAsFunction = medial_function_init,
		.attributes = kJSPropertyAttributeNone,
	},
	{
		.name = "register",
		.callAsFunction = medial_function_register,
		.attributes = kJSPropertyAttributeNone,
	},
	{
		.name = "login",
		.callAsFunction = medial_function_login,
		.attributes = kJSPropertyAttributeNone,
	},
	{
		.name = "logout",
		.callAsFunction = medial_function_logout,
		.attributes = kJSPropertyAttributeNone,
	},
	{}
};

static void medial_initialize(JSContextRef context, JSObjectRef object)
{
	struct js_medial *jsdg = JSObjectGetPrivate(object);

	jsdg->this = object;
}

static void medial_finalize(JSObjectRef object)
{
	struct js_medial *jsdg = JSObjectGetPrivate(object);

	medial_reset_channel(jsdg);
	if (jsdg->medial)
		g_free(jsdg->medial);

	g_source_destroy(&jsdg->source);
}

static const JSClassDefinition medial_classdef = {
	.className = "Medial",
	.initialize = medial_initialize,
	.finalize = medial_finalize,
	.staticValues = medial_properties,
	.staticFunctions = medial_functions,
};

static JSObjectRef javascript_medial_create(JSContextRef js, JSClassRef class,
		struct javascript_userdata *user_data)
{
	struct js_medial *jsdg;
	GSource *source;

	if (!user_data->rcd || !user_data->rcd->rc)
		return NULL;

	source = g_source_new(&js_medial_source_funcs, sizeof(*jsdg));
	jsdg = (struct js_medial *)source;
	if (!jsdg) {
		g_warning("js-medial: failed to allocate memory");
		return NULL;
	}

	jsdg->net_udp = remote_control_get_net_udp(user_data->rcd->rc);
	if (!jsdg->net_udp) {
		g_source_destroy(source);
		return NULL;
	}

	jsdg->udp_channel = -1;
	jsdg->cb_response = NULL;
	jsdg->cb_apply = NULL;
	jsdg->context = js;

	g_source_attach(source, g_main_loop_get_context(user_data->loop));

	return JSObjectMake(js, class, jsdg);
}

struct javascript_module javascript_medial = {
	.classdef = &medial_classdef,
	.create = javascript_medial_create,
};
