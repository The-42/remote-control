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

int32_t medcom_card_get_type(void *priv, enum medcom_card_type *type)
{
	struct smartcard *smartcard = remote_control_get_smartcard(priv);
	int32_t ret;

	g_debug("> %s(priv=%p, type=%p)", __func__, priv, type);

	ret = smartcard_get_type(smartcard, type);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_card_read(void *priv, off_t offset, struct rpc_buffer *buffer)
{
	struct smartcard *smartcard = remote_control_get_smartcard(priv);
	int32_t ret;

	g_debug("> %s(priv=%p, offset=%ld, buffer=%p)", __func__, priv, offset,
			buffer);

	ret = smartcard_read(smartcard, offset, buffer->tx_buf, buffer->tx_num);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_card_write(void *priv, off_t offset, struct rpc_buffer *buffer)
{
	struct smartcard *smartcard = remote_control_get_smartcard(priv);
	int32_t ret;

	g_debug("> %s(priv=%p, offset=%ld, buffer=%p)", __func__, priv, offset,
			buffer);

	ret = smartcard_write(smartcard, offset, buffer->rx_buf, buffer->rx_num);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}
