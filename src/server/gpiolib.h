/*
 * Copyright (C) 2012 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef GPIOLIB_H
#define GPIOLIB_H 1

#ifdef __cplusplus
extern "C" {
#endif

#define SYSFS_GPIO_PATH "/sys/class/gpio"

int gpio_request(unsigned int gpio);
int gpio_free(unsigned int gpio);
int gpio_direction_input(unsigned int gpio);
int gpio_direction_output(unsigned int gpio, int value);
int gpio_get_value(unsigned int gpio);
int gpio_set_value(unsigned int gpio, int value);

#ifdef __cplusplus
}
#endif

#endif /* GPIOLIB_H */
