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

int rfid_create(struct rfid **rfidp, struct rpc_server *server)
{
	return 0;
}

int rfid_free(struct rfid *rfid)
{
	return 0;
}

int rfid_get_type(struct rfid *rfid, unsigned int *typep)
{
	return -ENOSYS;
}

ssize_t rfid_read(struct rfid *rfid, off_t offset, void *buffer, size_t size)
{
	return -ENOSYS;
}

ssize_t rfid_write(struct rfid *rfid, off_t offset, const void *buffer,
		size_t size)
{
	return -ENOSYS;
}
