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

#include <glib.h>

#include "remote-control-stub.h"
#include "remote-control.h"

int32_t RPC_IMPL(check_io)(void *priv, uint8_t *value)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, value=%p)", __func__, priv, value);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(gpio_set_mask)(void *priv, uint32_t mask)
{
	struct gpio_backend *backend = remote_control_get_gpio_backend(RCPTR(priv));
	unsigned int num;
	unsigned int i;
	int32_t ret;
	int err;

	g_debug("> %s(priv=%p, mask=%#x)", __func__, priv, mask);
	g_debug("  mask: %08x", mask);

	err = gpio_backend_get_num_gpios(backend);
	if (err < 0) {
		ret = -err;
		goto out;
	}

	num = err;

	for (i = 0; i < num; i++) {
		int value = (mask >> i) & 1;

		ret = gpio_backend_direction_output(backend, i, value);
		if (ret < 0) {
			g_debug("gpio_backend_direction_output(%u) failed: %s",
					i, strerror(-ret));
		}
	}

	ret = 0;

out:
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t RPC_IMPL(gpio_get_mask)(void *priv, uint32_t *mask)
{
	struct gpio_backend *backend = remote_control_get_gpio_backend(RCPTR(priv));
	uint32_t value = 0;
	unsigned int num;
	unsigned int i;
	int32_t ret;
	int err;

	g_debug("> %s(priv=%p, mask=%p)", __func__, priv, mask);

	err = gpio_backend_get_num_gpios(backend);
	if (err < 0) {
		ret = -err;
		goto out;
	}

	num = err;

	for (i = 0; i < num; i++) {
		ret = gpio_backend_get_value(backend, i);
		if (ret < 0) {
			g_debug("gpio_backend_get_value(%u) failed: %s",
					i, strerror(-ret));
			continue;
		}

		if (ret)
			value |= BIT(i);
	}

	if (mask)
		*mask = value;

	g_debug("  mask: %08x", value);
	ret = 0;

out:
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}
