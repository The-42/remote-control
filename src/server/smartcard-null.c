/*
 * Copyright (C) 2010-2012 Avionic Design GmbH
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

int smartcard_create(struct smartcard **smartcardp, struct rpc_server *server,
		     GKeyFile *config)
{
	return 0;
}

int smartcard_free(struct smartcard *smartcard)
{
	return 0;
}

int smartcard_get_type(struct smartcard *smartcard, unsigned int *typep)
{
	return -ENOSYS;
}

ssize_t smartcard_read(struct smartcard *smartcard, off_t offset, void *buffer,
		size_t size)
{
	return -ENOSYS;
}

ssize_t smartcard_write(struct smartcard *smartcard, off_t offset,
		const void *buffer, size_t size)
{
	return -ENOSYS;
}
