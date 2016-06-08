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

struct smartcard_data;

#define REGISTER_SMARTCARD(TYPE)                                               \
	int smartcard_create_##TYPE(struct smartcard_data **smartcardp,        \
		struct rpc_server *server, GKeyFile *config);                  \
	int smartcard_free_##TYPE(struct smartcard_data *smartcard);           \
	int smartcard_get_type_##TYPE(struct smartcard_data *smartcard_data,   \
		unsigned int *typep);                                          \
	int smartcard_get_type_##TYPE(struct smartcard_data *smartcard,        \
		unsigned int *typep);                                          \
	ssize_t smartcard_read_##TYPE(struct smartcard_data *smartcard,        \
			off_t offset, void *buffer, size_t size);              \
	ssize_t smartcard_write_##TYPE(struct smartcard_data *smartcard,       \
		off_t offset, const void *buffer, size_t size)

#define SET_SMARTCARD(val,TYPE)                                                \
	val->p_free     = smartcard_free_##TYPE;                               \
	val->p_get_type = smartcard_get_type_##TYPE;                           \
	val->p_read     = smartcard_read_##TYPE;                               \
	val->p_write    = smartcard_write_##TYPE

#if ENABLE_LIBPCSCLITE
REGISTER_SMARTCARD(pcsc);
#endif
#if ENABLE_LIBSMARTCARD
REGISTER_SMARTCARD(i2c);
#endif

struct smartcard {
	struct smartcard_data *data;;
	int (*p_free)(struct smartcard_data *smartcardp);
	int (*p_get_type)(struct smartcard_data *smartcard,
		unsigned int *typep);
	ssize_t (*p_read)(struct smartcard_data *smartcard,
		off_t offset, void *buffer, size_t size);
	ssize_t (*p_write)(struct smartcard_data *smartcard,
		off_t offset, const void *buffer, size_t size);
};

int smartcard_create(struct smartcard **smartcardp, struct rpc_server *server,
		     GKeyFile *config)
{
	int ret = 0;
	struct smartcard *smartcard;

	if (!smartcardp)
		return -EINVAL;

	if (!g_key_file_has_group(config, "smartcard"))
		return -EIO;

	smartcard = malloc(sizeof(*smartcard));
	if (!smartcard)
		return -ENOMEM;

	memset(smartcard, 0, sizeof(*smartcard));
	*smartcardp = smartcard;

	//TODO: Force type if defined by configuration
#if ENABLE_LIBPCSCLITE
	ret = smartcard_create_pcsc(&smartcard->data, server, config);
	if (ret >= 0) {
		SET_SMARTCARD(smartcard, pcsc);
		return ret;
	}
#endif
#if ENABLE_LIBSMARTCARD
	ret = smartcard_create_i2c(&smartcard->data, server, config);
	if (ret >= 0) {
		SET_SMARTCARD(smartcard, i2c);
		return ret;
	}
#endif
	return 0;
}

int smartcard_free(struct smartcard *smartcard)
{
	if (!smartcard)
		return -EINVAL;

	if (smartcard->p_free)
		smartcard->p_free(smartcard->data);

	free(smartcard);
	return 0;
}

int smartcard_get_type(struct smartcard *smartcard, unsigned int *typep)
{
	if (!smartcard || !typep)
		return -EINVAL;

	if (smartcard->p_get_type)
		return smartcard->p_get_type(smartcard->data, typep);

	return -ENOSYS;
}

ssize_t smartcard_read(struct smartcard *smartcard, off_t offset, void *buffer,
		size_t size)
{
	if (!smartcard)
		return -EINVAL;

	if (smartcard->p_read)
		return smartcard->p_read(smartcard->data, offset, buffer, size);

	return -ENOSYS;
}

ssize_t smartcard_write(struct smartcard *smartcard, off_t offset,
		const void *buffer, size_t size)
{
	if (!smartcard)
		return -EINVAL;

	if (smartcard->p_write)
		return smartcard->p_write(smartcard->data, offset, buffer, size);

	return -ENOSYS;
}
