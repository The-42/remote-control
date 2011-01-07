/*
 * Copyright (C) 2010-2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "remote-control.h"

int32_t medcom_card_get_type(void *priv, enum medcom_card_type *type)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int err;

	err = medcom_card_get_type_stub(rpc, &ret, type);
	if (err < 0) {
		ret = err;
		goto out;
	}

out:
	return ret;
}

int32_t medcom_card_read(void *priv, off_t offset, void *buffer, size_t size)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	struct rpc_buffer buf;
	int32_t ret = 0;
	int err = 0;

	buf.tx_buf = NULL;
	buf.tx_num = 0;
	buf.rx_buf = buffer;
	buf.rx_num = size;

	err = medcom_card_read_stub(rpc, &ret, offset, &buf);
	if (err < 0) {
		ret = err;
		goto out;
	}

out:
	return ret;
}

int32_t medcom_card_write(void *priv, off_t offset, const void *buffer,
		size_t size)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	struct rpc_buffer buf;
	int32_t ret = 0;
	int err = 0;

	buf.tx_buf = (char *)buffer;
	buf.tx_num = size;
	buf.rx_buf = NULL;
	buf.rx_num = 0;

	err = medcom_card_write_stub(rpc, &ret, offset, &buf);
	if (err < 0) {
		ret = err;
		goto out;
	}

out:
	return ret;
}
