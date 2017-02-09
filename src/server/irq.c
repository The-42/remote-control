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

#include <glib.h>

#include "remote-control-stub.h"
#include "remote-control.h"

#define BIT(x) (1 << (x))

enum {
	IRQ_HOOK,
	IRQ_SMARTCARD,
	IRQ_VOIP,
	IRQ_IO,
	IRQ_MODEM,
	IRQ_HANDSET,
};

int32_t RPC_IMPL(irq_enable)(void *priv, uint8_t virtkey)
{
	struct rpc_server *server = rpc_server_from_priv(priv);
	int err;

	err = RPC_STUB(irq_event)(server, 0);
	if (err < 0) {
		g_debug("irq_event(): %s", strerror(-err));
		return err;
	}

	return 0;
}

int32_t RPC_IMPL(irq_get_mask)(void *priv, uint32_t *mask)
{
	struct event_manager *manager = remote_control_get_event_manager(priv);
	uint32_t status = 0;
	int32_t ret;

	g_debug("> %s(priv=%p, mask=%p)", __func__, priv, mask);

	if (!priv || !mask) {
		ret = -EINVAL;
		goto out;
	}

	ret = event_manager_get_status(manager, &status);
	g_debug("  event_manager_get_status(): %d", ret);
	g_debug("  status: %08x", status);

	if (status & BIT(EVENT_SOURCE_MODEM))
		*mask |= BIT(IRQ_MODEM);

	if (status & BIT(EVENT_SOURCE_VOIP))
		*mask |= BIT(IRQ_VOIP);

	if (status & BIT(EVENT_SOURCE_SMARTCARD))
		*mask |= BIT(IRQ_SMARTCARD);

	if (status & BIT(EVENT_SOURCE_HOOK))
		*mask |= BIT(IRQ_HOOK);

	if (status & BIT(EVENT_SOURCE_HANDSET))
		*mask |= BIT(IRQ_HANDSET);

out:
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(irq_get_info)(void *priv, enum RPC_TYPE(irq_source) source, uint32_t *info)
{
	struct event_manager *manager = remote_control_get_event_manager(priv);
	struct event event;
	int32_t ret = 0;
	int err;

	g_debug("> %s(priv=%p, source=%d, info=%p)", __func__, priv, source, info);

	memset(&event, 0, sizeof(event));

	switch (source) {
	case RPC_MACRO(IRQ_SOURCE_UNKNOWN):
		g_debug("  IRQ_SOURCE_UNKNOWN");
		break;

	case RPC_MACRO(IRQ_SOURCE_HOOK):
		g_debug("  IRQ_SOURCE_HOOK");
		event.source = EVENT_SOURCE_HOOK;

		err = event_manager_get_source_state(manager, &event);
		if (err < 0) {
			ret = err;
			break;
		}

		switch (event.hook.state) {
		case EVENT_HOOK_STATE_OFF:
			g_debug("    EVENT_HOOK_STATE_OFF");
			*info = 1;
			break;

		case EVENT_HOOK_STATE_ON:
			g_debug("    EVENT_HOOK_STATE_ON");
			*info = 0;
			break;

		default:
			ret = -ENXIO;
			break;
		}
		break;

	case RPC_MACRO(IRQ_SOURCE_CARD):
		g_debug("  IRQ_SOURCE_CARD");
		event.source = EVENT_SOURCE_SMARTCARD;

		err = event_manager_get_source_state(manager, &event);
		if (err < 0) {
			ret = err;
			break;
		}

		switch (event.smartcard.state) {
		case EVENT_SMARTCARD_STATE_INSERTED:
			*info = 1;
			break;

		case EVENT_SMARTCARD_STATE_REMOVED:
			*info = 0;
			break;

		default:
			ret = -ENXIO;
			break;
		}
		break;

	case RPC_MACRO(IRQ_SOURCE_VOIP):
		g_debug("  IRQ_SOURCE_VOIP");
		event.source = EVENT_SOURCE_VOIP;

		err = event_manager_get_source_state(manager, &event);
		if (err < 0) {
			ret = err;
			break;
		}

		switch (event.voip.state) {
		case EVENT_VOIP_STATE_IDLE:
			g_debug("    EVENT_VOIP_STATE_IDLE");
			*info = 0; /* VOIP_EVT_IDLE */
			break;

		case EVENT_VOIP_STATE_LOGGED_ON:
			g_debug("    EVENT_VOIP_STATE_LOGGED_ON");
			*info = 1; /* VOIP_EVT_LOGON */
			break;

		case EVENT_VOIP_STATE_LOGGED_OFF:
			g_debug("    EVENT_VOIP_STATE_LOGGED_OFF");
			*info = 2; /* VOIP_EVT_LOGOFF */
			break;

		case EVENT_VOIP_STATE_OUTGOING:
			g_debug("    EVENT_VOIP_STATE_OUTGOING");
			*info = 5; /* VOIP_EVT_TRYING */
			break;

		case EVENT_VOIP_STATE_OUTGOING_CONNECTED:
			g_debug("    EVENT_VOIP_STATE_OUTGOING_CONNECTED");
			*info = 3; /* VOIP_EVT_CONNECT */
			break;

		case EVENT_VOIP_STATE_OUTGOING_DISCONNECTED:
			g_debug("    EVENT_VOIP_STATE_OUTGOING_DISCONNECTED");
			*info = 4; /* VOIP_EVT_DISCONNECT */
			break;

		case EVENT_VOIP_STATE_INCOMING:
			g_debug("    EVENT_VOIP_STATE_INCOMING");
			*info = 7; /* VOIP_EVT_INCOMMING [sic] */
			break;

		case EVENT_VOIP_STATE_INCOMING_CONNECTED:
			g_debug("    EVENT_VOIP_STATE_INCOMING_CONNECTED");
			*info = 3; /* VOIP_EVT_CONNECT */
			break;

		case EVENT_VOIP_STATE_INCOMING_DISCONNECTED:
			g_debug("    EVENT_VOIP_STATE_INCOMING_DISCONNECTED");
			*info = 4; /* VOIP_EVT_DISCONNECT */
			break;

		case EVENT_VOIP_STATE_INCOMING_MISSED:
			g_debug("    EVENT_VOIP_STATE_INCOMING_MISSED");
			*info = 8; /* VOIP_EVT_MISSEDCALL */
			break;

		case EVENT_VOIP_STATE_ERROR_USER_BUSY:
			*info = 9; /* VOIP_EVT_OUT_RINGING -> used as user busy*/
			break;

		case EVENT_VOIP_STATE_OUTGOING_EARLYMEDIA:
			g_debug("    EVENT_VOIP_STATE_OUTGOING_EARLYMEDIA");
			*info = 10; /* undefined under windows ce */
			break;

		case EVENT_VOIP_STATE_INCOMING_EARLYMEDIA:
			g_debug("    EVENT_VOIP_STATE_INCOMING_EARLYMEDIA");
			*info = 11; /* undefined under windows ce */
			break;

		default:
			ret = -ENXIO;
			break;
		}
		break;

	case RPC_MACRO(IRQ_SOURCE_IO):
		g_debug("  IRQ_SOURCE_IO");
		break;

	case RPC_MACRO(IRQ_SOURCE_MODEM):
		g_debug("  IRQ_SOURCE_MODEM");
		event.source = EVENT_SOURCE_MODEM;

		err = event_manager_get_source_state(manager, &event);
		if (err < 0) {
			ret = err;
			break;
		}

		switch (event.modem.state) {
		case EVENT_MODEM_STATE_RINGING:
			*info = 7;
			break;

		case EVENT_MODEM_STATE_CONNECTED:
			*info = 3;
			break;

		case EVENT_MODEM_STATE_DISCONNECTED:
			*info = 4;
			break;

		case EVENT_MODEM_STATE_ERROR:
			*info = -10;
			break;

		default:
			ret = -ENXIO;
			break;
		}
		break;

	case RPC_MACRO(IRQ_SOURCE_HANDSET):
		g_debug("  IRQ_SOURCE_HANDSET");
		event.source = EVENT_SOURCE_HANDSET;

		err = event_manager_get_source_state(manager, &event);
		if (err < 0) {
			ret = err;
			break;
		}

		*info = event.handset.keycode & 0xffff;

		if (event.handset.pressed)
			*info |= (1 << 16);

		break;

	default:
		ret = -EINVAL;
		break;
	}

	g_debug("< %s(info=%x) = %d", __func__, *info, ret);
	return ret;
}
