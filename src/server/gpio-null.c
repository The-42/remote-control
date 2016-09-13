/*
 * Copyright (C) 2011-2012 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "remote-control.h"

int gpio_backend_create(struct gpio_backend **backendp, struct event_manager *events,
		     GKeyFile *config)
{
	return 0;
}

int gpio_backend_free(struct gpio_backend *backend)
{
	return 0;
}

GSource *gpio_backend_get_source(struct gpio_backend *backend)
{
	return NULL;
}

int gpio_backend_get_num_gpios(struct gpio_backend *backend)
{
	return 0;
}

int gpio_backend_direction_input(struct gpio_backend *backend, unsigned int gpio)
{
	return -ENOSYS;
}

int gpio_backend_direction_output(struct gpio_backend *backend, unsigned int gpio,
		int value)
{
	return -ENOSYS;
}

int gpio_backend_set_value(struct gpio_backend *backend, unsigned int gpio, int value)
{
	return -ENOSYS;
}

int gpio_backend_get_value(struct gpio_backend *backend, unsigned int gpio)
{
	return -ENOSYS;
}
