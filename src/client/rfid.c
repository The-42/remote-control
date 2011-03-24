/*
 * Copyright (C) 2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "remote-control-stub.h"
#include "remote-control.h"

remote_public
int remote_rfid_get_type(void *priv, enum remote_rfid_type *typep)
{
	enum RPC_TYPE(rfid_type) type = RPC_MACRO(RFID_TYPE_UNKNOWN);
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int err;

	err = RPC_STUB(rfid_get_type)(rpc, &ret, &type);
	if (err < 0) {
		ret = err;
		goto out;
	}

	/*
	 * TODO: Use a proper mapping between remote_rfid_type and
	 *       RPC_TYPE(rfid_type). This currently works because both
	 *       enumerations are defined identically.
	 */
	*typep = type;

out:
	return ret;
}

remote_public
int remote_rfid_read(void *priv, off_t offset, void *buffer, size_t size)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	struct rpc_buffer buf;
	int32_t ret = 0;
	int err = 0;

	buf.tx_buf = NULL;
	buf.tx_num = 0;
	buf.rx_buf = buffer;
	buf.rx_num = size;

	err = RPC_STUB(rfid_read)(rpc, &ret, offset, &buf);
	if (err < 0) {
		ret = err;
		goto out;
	}

out:
	return ret;
}

remote_public
int remote_rfid_write(void *priv, off_t offset, const void *buffer,
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

	err = RPC_STUB(rfid_write)(rpc, &ret, offset, &buf);
	if (err < 0) {
		ret = err;
		goto out;
	}

out:
	return ret;
}
