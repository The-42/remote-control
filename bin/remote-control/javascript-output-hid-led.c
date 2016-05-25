/*
 * Copyright (C) 2014 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <errno.h>
#include <math.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <linux/input.h>

#include "find-device.h"
#include "javascript.h"
#include "javascript-output.h"

struct js_output {
	char *path;
	char *name;
	int code;
};

static const struct {
	const char *name;
	int code;
} led_event_codes[] = {
	/* Add the generated name to code mapping list */
	#include "javascript-input-led-codes.c"
	{}
};

static int get_led_code(const char *name)
{
	int i;

	if (!name)
		return -EINVAL;

	for (i = 0; led_event_codes[i].name; i++)
		if (!strcmp(name, led_event_codes[i].name))
			return led_event_codes[i].code;

	return -ENOENT;
}

static int js_output_hid_led_set(struct js_output *out, double value)
{
	struct input_event ev = {};
	int err;
	int fd;

	if (!out->path)
		return -ENODEV;

	fd = open(out->path, O_WRONLY);
	if (fd < 0)
		return -errno;

	ev.type = EV_LED;
	ev.code = out->code;
	ev.value = value > 0;

	err = write(fd, &ev, sizeof(ev));
	close(fd);

	return err < 0 ? -errno : 0;
}

static int js_output_hid_led_get(struct js_output *out, double *valuep)
{
	uint8_t leds[16] = {};
	int err;
	int fd;

	if (!out->path)
		return -ENODEV;

	if (out->code / 8 >= sizeof(leds)) {
		g_critical("%s: Output %s has a too large LED code",
			__func__, out->name ? out->name : out->path);
		return -EINVAL;
	}

	fd = open(out->path, O_RDONLY);
	if (fd < 0)
		return -errno;

	err = ioctl(fd, EVIOCGLED(sizeof(leds)), leds);
	if (err > 0)
		*valuep = (leds[out->code >> 3] >> (out->code & 7)) & 1;
	close(fd);
	return err < 0 ? -errno : 0;
}

static int on_input_device_found(gpointer user, const gchar *filename,
				const gchar *name, const int vendorId,
				const int productId)
{
	struct js_output *out = user;

	/* Skip other devices once we got one */
	if (!out->path)
		out->path = g_strdup(filename);
	return 0;
}

static int js_output_hid_led_prepare(struct js_output *out)
{
	int err;

	if (!out->name)
		return 0;

	if (out->path) {
		g_free(out->path);
		out->path = NULL;
	}

	err = find_input_devices(out->name, on_input_device_found, out);
	return err == 0 ? -ENOENT : err;
}

static int js_output_hid_led_create(
	GKeyFile *config, const char* name, struct js_output **outp)
{
	struct js_output *output;
	char *led_name = NULL;
	char *dev_name = NULL;
	char *path = NULL;
	int led_code;
	int err;

	if (!config || !name || !outp)
		return -EINVAL;

	dev_name = javascript_config_get_string(config, OUTPUT_GROUP, name,
						"device-name");
	path = javascript_config_get_string(config, OUTPUT_GROUP, name,
					"path");
	if (!path && !dev_name) {
		g_critical("%s: Output %s doesn't have a path or device name",
			__func__, name);
		err = -EINVAL;
		goto on_error;
	}

	led_name = javascript_config_get_string(config, OUTPUT_GROUP, name, "led");
	if (!led_name) {
		g_critical("%s: Output %s is missing the 'led' key",
			__func__, name);
		err = -EINVAL;
		goto on_error;
	}

	led_code = get_led_code(led_name);
	if (led_code < 0) {
		g_critical("%s: Output %s has an unknown led: %s",
			__func__, name, led_name);
		err = -EINVAL;
		goto on_error;
	}

	output = g_malloc0(sizeof(*output));
	if (!output) {
		err = -ENOMEM;
		goto on_error;
	}

	output->path = path;
	output->name = dev_name;
	output->code = led_code;

	*outp = output;

	g_free(led_name);
	return 0;

on_error:
	g_free(dev_name);
	g_free(led_name);
	g_free(path);
	return err;
}


const struct js_output_type js_output_hid_led = {
	.name = "hid-led",
	.create = js_output_hid_led_create,
	.prepare = js_output_hid_led_prepare,
	.set = js_output_hid_led_set,
	.get = js_output_hid_led_get,
};
