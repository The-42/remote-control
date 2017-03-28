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
#include "rcrpc.h"

int32_t RPC_IMPL(card_get_type)(void *priv, enum RPC_TYPE(card_type) *typep)
{
	struct smartcard *smartcard = remote_control_get_smartcard(RCPTR(priv));
	enum smartcard_type type;
	int32_t ret;

	g_debug("> %s(priv=%p, type=%p)", __func__, priv, typep);

	ret = smartcard_get_type(smartcard, &type);
	if (ret < 0)
		goto out;

	switch (type) {
	case SMARTCARD_TYPE_I2C:
		*typep = RPC_MACRO(CARD_TYPE_I2C);
		break;
	case SMARTCARD_TYPE_T0:
		*typep = RPC_MACRO(CARD_TYPE_T0);
		break;
	case SMARTCARD_TYPE_T1:
		*typep = RPC_MACRO(CARD_TYPE_T1);
		break;
	default:
		*typep = RPC_MACRO(CARD_TYPE_UNKNOWN);
		break;
	}

out:
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(card_read)(void *priv, off_t offset, struct rpc_buffer *buffer)
{
	struct smartcard *smartcard = remote_control_get_smartcard(RCPTR(priv));
	int32_t ret;

	g_debug("> %s(priv=%p, offset=%ld, buffer=%p)", __func__, priv, offset,
			buffer);

	ret = smartcard_read(smartcard, offset, buffer->tx_buf, buffer->tx_num);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(card_write)(void *priv, off_t offset, struct rpc_buffer *buffer)
{
	struct smartcard *smartcard = remote_control_get_smartcard(RCPTR(priv));
	int32_t ret;

	g_debug("> %s(priv=%p, offset=%ld, buffer=%p)", __func__, priv, offset,
			buffer);

	ret = smartcard_write(smartcard, offset, buffer->rx_buf, buffer->rx_num);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}
