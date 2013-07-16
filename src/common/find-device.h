/*
 * Copyright (C) 2013 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __FIND_DEVICE_H__
#define __FIND_DEVICE_H__

typedef int (*device_found_cb)(gpointer user, const gchar *name);

/**
 * @param devname  The name of the device.
 * @param function The function to call if device was found.
 * @param user     User data passed to function.
 */
gint
find_input_devices(const gchar *devname, device_found_cb callback,
		   gpointer user);

#endif /* __FIND_DEVICE_H__ */
