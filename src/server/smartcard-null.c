/*
 * Copyright (C) 2010-2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "remote-control-stub.h"
#include "remote-control.h"

int smartcard_create(struct smartcard **smartcardp)
{
	return 0;
}

int smartcard_free(struct smartcard *smartcard)
{
	return 0;
}

int smartcard_get_type(struct smartcard *smartcard, unsigned int *typep)
{
	if (typep)
		*typep = CARD_TYPE_NONE;

	return 0;
}

ssize_t smartcard_read(struct smartcard *smartcard, off_t offset, void *buffer,
		size_t size)
{
	return -ENODEV;
}

ssize_t smartcard_write(struct smartcard *smartcard, off_t offset,
		const void *buffer, size_t size)
{
	return -ENODEV;
}
