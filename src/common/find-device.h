/*
 * Copyright (C) 2013 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __FIND_DEVICE_H__
#define __FIND_DEVICE_H__

#include <gudev/gudev.h>

/** Define to create match type IDs
 *
 * To make the implementation simpler the lower 16 bits of the
 * ID are used to store a few flags about the type.
 */
#define UDEV_MATCH_TYPE(op, flags) (((op) << 16) | (flags))

/** Match type flag for inverted matches */
#define UDEV_MATCH_NOT		1
/** Match type flag for invertable matches */
#define UDEV_MATCH_HAS_NOT	2
/** Match type flag for types that need a key beside the value */
#define UDEV_MATCH_HAS_KEY	4

/** Enum of the possible match types */
enum udev_match_type {
	/** Match all devices from a subsystem */
	UDEV_MATCH_SUBSYSTEM = UDEV_MATCH_TYPE(
		1, UDEV_MATCH_HAS_NOT),
	/** Match all devices not from a subsystem */
	UDEV_MATCH_NOT_SUBSYSTEM = UDEV_MATCH_TYPE(
		1, UDEV_MATCH_HAS_NOT | UDEV_MATCH_NOT),
	/** Match all devices with a sysfs attribute */
	UDEV_MATCH_SYSFS_ATTR = UDEV_MATCH_TYPE(
		2, UDEV_MATCH_HAS_NOT | UDEV_MATCH_HAS_KEY),
	/** Match all devices without a sysfs attribute */
	UDEV_MATCH_NOT_SYSFS_ATTR = UDEV_MATCH_TYPE(
		2, UDEV_MATCH_HAS_NOT | UDEV_MATCH_NOT | UDEV_MATCH_HAS_KEY),
	/** Match all devices with a udev property */
	UDEV_MATCH_PROPERTY = UDEV_MATCH_TYPE(
		3, UDEV_MATCH_HAS_KEY),
	/** Match all devices with a given kernel name */
	UDEV_MATCH_NAME = UDEV_MATCH_TYPE(
		4, 0),
	/** Match all devices with a given udev tag */
	UDEV_MATCH_TAG  = UDEV_MATCH_TYPE(
		5, 0),
};

/** Struct to hold a matching rule */
struct udev_match {
	/** Type of the match */
	enum udev_match_type type;
	/** Optional match key, only used for some types */
	char *key;
	/** Value to match */
	char *value;
};

/**
 * Callback run when an input device has been found
 *
 * @param user The user data passed to the find function
 * @param name The path of the device found
 * @return     A negative value to stop calling the callback,
 *             zero or a positive value to continue
 */
typedef int (*udev_device_found_cb)(gpointer user, GUdevDevice *dev);

/**
 * Lookup devices in udev that match a set of criteria
 *
 * @param match    An array of matching critera
 * @param count    The number of matching critera in the array
 * @param callback The callback to call when a device has been found
 * @param user     Userdata pointer to pass to the callback
 * @return         The number of devices found, otherwise a negative
 *                 error code
 */
gint find_udev_devices(const struct udev_match *match,
		udev_device_found_cb callback, gpointer user);

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
