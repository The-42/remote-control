/*
 * Copyright (C) 2016 Avionic Design GmbH
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

int smartcard_read_info(struct smartcard *smartcard,
		struct smartcard_info *data)
{
	return -ENOSYS;
}

void smartcard_read_info_free(struct smartcard_info *data)
{
}
