/*
 * Copyright (C) 2010-2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <glib.h>

#include "remote-control-stub.h"
#include "remote-control.h"

#define BIT(x) (1 << (x))

enum {
	IRQ_HANDSET,
	IRQ_SMARTCARD,
	IRQ_VOIP,
	IRQ_IO,
	IRQ_MODEM,
};

int32_t medcom_irq_enable(void *priv, uint8_t virtkey)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, virtkey=%#x)", __func__, priv, virtkey);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_irq_get_mask(void *priv, uint32_t *mask)
{
	struct remote_control *rc = priv;
	uint32_t status = 0;
	int32_t ret;

	g_debug("> %s(priv=%p, mask=%p)", __func__, priv, mask);

	if (!priv || !mask) {
		ret = -EINVAL;
		goto out;
	}

	ret = event_manager_get_status(rc->event_manager, &status);
	g_debug("  event_manager_get_status(): %d", ret);
	g_debug("  status: %08x", status);

	if (status & BIT(EVENT_SOURCE_VOIP))
		*mask |= BIT(IRQ_VOIP);

	if (status & BIT(EVENT_SOURCE_SMARTCARD))
		*mask |= BIT(IRQ_SMARTCARD);

	if (status & BIT(EVENT_SOURCE_HANDSET))
		*mask |= BIT(IRQ_HANDSET);

out:
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_irq_get_info(void *priv, enum medcom_irq_source source, uint32_t *info)
{
	struct remote_control *rc = priv;
	struct event event;
	int32_t ret = 0;
	int err;

	g_debug("> %s(priv=%p, source=%d, info=%p)", __func__, priv, source, info);

	memset(&event, 0, sizeof(event));

	switch (source) {
	case MEDCOM_IRQ_SOURCE_UNKNOWN:
		g_debug("  MEDCOM_IRQ_SOURCE_UNKNOWN");
		break;

	case MEDCOM_IRQ_SOURCE_HOOK:
		g_debug("  MEDCOM_IRQ_SOURCE_HOOK");
		event.source = EVENT_SOURCE_HANDSET;

		err = event_manager_get_source_state(rc->event_manager, &event);
		if (err < 0) {
			ret = err;
			break;
		}

		switch (event.handset.state) {
		case EVENT_HANDSET_STATE_HOOK_OFF:
			g_debug("    EVENT_HANDSET_STATE_HOOK_OFF");
			*info = 1;
			break;

		case EVENT_HANDSET_STATE_HOOK_ON:
			g_debug("    EVENT_HANDSET_STATE_HOOK_ON");
			*info = 0;
			break;

		default:
			ret = -ENXIO;
			break;
		}
		break;

	case MEDCOM_IRQ_SOURCE_CARD:
		g_debug("  MEDCOM_IRQ_SOURCE_CARD");
		event.source = EVENT_SOURCE_SMARTCARD;

		err = event_manager_get_source_state(rc->event_manager, &event);
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

	case MEDCOM_IRQ_SOURCE_VOIP:
		g_debug("  MEDCOM_IRQ_SOURCE_VOIP");
		event.source = EVENT_SOURCE_VOIP;

		err = event_manager_get_source_state(rc->event_manager, &event);
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
			*info = 6; /* VOIP_EVT_CALLING */
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

		default:
			ret = -ENXIO;
			break;
		}
		break;

	case MEDCOM_IRQ_SOURCE_IO:
		g_debug("  MEDCOM_IRQ_SOURCE_IO");
		break;

	case MEDCOM_IRQ_SOURCE_RDP:
		g_debug("  MEDCOM_IRQ_SOURCE_RDP");
		break;

	default:
		ret = -EINVAL;
		break;
	}

	g_debug("< %s(info=%x) = %d", __func__, *info, ret);
	return ret;
}
