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

#include "remote-control-stub.h"
#include "remote-control.h"

int gpio_chip_create(struct gpio_chip **chipp, struct event_manager *events,
		     GKeyFile *config)
{
	return 0;
}

int gpio_chip_free(struct gpio_chip *chip)
{
	return 0;
}

GSource *gpio_chip_get_source(struct gpio_chip *chip)
{
	return NULL;
}

int gpio_chip_get_num_gpios(struct gpio_chip *chip)
{
	return 0;
}

int gpio_chip_direction_input(struct gpio_chip *chip, unsigned int gpio)
{
	return -ENOSYS;
}

int gpio_chip_direction_output(struct gpio_chip *chip, unsigned int gpio,
		int value)
{
	return -ENOSYS;
}

int gpio_chip_set_value(struct gpio_chip *chip, unsigned int gpio, int value)
{
	return -ENOSYS;
}

int gpio_chip_get_value(struct gpio_chip *chip, unsigned int gpio)
{
	return -ENOSYS;
}
