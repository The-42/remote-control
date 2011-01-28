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
int32_t medcom_lldp_read(void *priv, void *data, size_t size)
{
	struct rpc_client *client = rpc_client_from_priv(priv);
	struct rpc_buffer buffer;
	int32_t ret;
	int err;

	memset(&buffer, 0, sizeof(buffer));
	buffer.rx_buf = data;
	buffer.rx_num = size;

	err = medcom_lldp_read_stub(client, &ret, 0, &buffer);
	if (err < 0)
		ret = err;

	return ret;
}
