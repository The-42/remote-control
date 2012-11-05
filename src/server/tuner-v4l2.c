/*
 * Copyright (C) 2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

#include "remote-control-stub.h"
#include "remote-control.h"

static const char V4L2_DEVICE[] = "/dev/video0";

static int get_tuner(int fd, struct v4l2_tuner *tuner)
{
	struct v4l2_input input;
	int err;

	if ((fd < 0) || !tuner)
		return -EINVAL;

	memset(&input, 0, sizeof(input));
	input.index = 0;

	err = ioctl(fd, VIDIOC_G_INPUT, &input);
	if (err < 0)
		return -ENODEV;

	err = ioctl(fd, VIDIOC_ENUMINPUT, &input);
	if (err < 0)
		return -errno;

	if (input.type != V4L2_INPUT_TYPE_TUNER)
		return -ENODEV;

	memset(tuner, 0, sizeof(*tuner));
	tuner->index = input.tuner;

	err = ioctl(fd, VIDIOC_G_TUNER, tuner);
	if (err < 0)
		return -errno;

	return 0;
}

int tuner_create(struct tuner **tunerp)
{
	return 0;
}

int tuner_free(struct tuner *tuner)
{
	return 0;
}

int tuner_set_input(struct tuner *tuner, int input_nr)
{
	int v4l2_fd;
	int err;

	v4l2_fd = open(V4L2_DEVICE, O_RDWR);
	if (v4l2_fd < 0) {
		err = -errno;
		g_debug("  failed to open %s: %s", V4L2_DEVICE, strerror(-err));
		goto out;
	}

	err = ioctl(v4l2_fd, VIDIOC_S_INPUT, &input_nr);
	if (err < 0) {
		g_debug("   ioctl(VIDIOC_S_INPUT): %s", strerror(-err));
		goto close;
	}
close:
	close(v4l2_fd);
out:
	return err;
}

int tuner_set_standard(struct tuner *tuner, const gchar *standard)
{
	v4l2_std_id std_id = V4L2_STD_PAL_BG;
	int v4l2_fd;
	int err;

	v4l2_fd = open(V4L2_DEVICE, O_RDWR);
	if (v4l2_fd < 0) {
		err = -errno;
		g_debug("  failed to open %s: %s", V4L2_DEVICE, strerror(-err));
		goto out;
	}

	if (g_ascii_strncasecmp(standard, "PAL-BG", 6) == 0)
		std_id = V4L2_STD_PAL_BG;
	else if (g_ascii_strncasecmp(standard, "PAL-B", 5) == 0)
		std_id = V4L2_STD_PAL_B;
	else if (g_ascii_strncasecmp(standard, "PAL-G", 5) == 0)
		std_id = V4L2_STD_PAL_G;
	else {
		g_debug("Unknwon v4l2 standard: %s", standard);
		err = -EINVAL;
		goto close;
	}

	err = ioctl(v4l2_fd, VIDIOC_S_STD, &std_id);
	if (err < 0) {
		g_debug("   ioctl(VIDIOC_S_STD): %s", strerror(-err));
		exit (EXIT_FAILURE);
	}
close:
	close(v4l2_fd);
out:
	return err;
}

int tuner_set_frequency(struct tuner *tuner, unsigned long frequency)
{
	struct v4l2_frequency freq;
	struct v4l2_tuner input;
	int v4l2_fd;
	int err;

	g_debug("> %s(tuner=%p, frequency=%lu)", __func__, tuner, frequency);

	v4l2_fd = open(V4L2_DEVICE, O_RDWR);
	if (v4l2_fd < 0) {
		err = -errno;
		g_debug("  failed to open %s: %s", V4L2_DEVICE, strerror(-err));
		goto out;
	}

	err = get_tuner(v4l2_fd, &input);
	if (err < 0) {
		g_debug("  failed to get tuner: %s", strerror(-err));
		goto close;
	}

	memset(&freq, 0, sizeof(freq));
	freq.tuner = input.index;
	freq.type = input.type;

	if (input.capability & V4L2_TUNER_CAP_LOW)
		freq.frequency = frequency / 62500;
	else
		freq.frequency = (frequency * 10) / 625;

	err = ioctl(v4l2_fd, VIDIOC_S_FREQUENCY, &freq);
	if (err < 0) {
		err = -errno;
		g_debug("  ioctl(VIDIOC_S_FREQUENCY): %s", strerror(-err));
		goto close;
	}

close:
	close(v4l2_fd);
out:
	return err;
}
