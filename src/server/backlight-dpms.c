/*
 * Copyright (C) 2010-2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <X11/Xlib.h>
#include <X11/extensions/dpms.h>

#include <glib.h>

#include "remote-control-stub.h"
#include "remote-control.h"

struct backlight {
	Display *display;
	int dpms;
};

int backlight_create(struct backlight **backlightp)
{
	struct backlight *backlight;
	int dummy = 0;
	int major = 0;
	int minor = 0;

	if (!backlightp)
		return -EINVAL;

	backlight = calloc(1, sizeof(*backlight));
	if (!backlight)
		return -ENOMEM;

	backlight->display = XOpenDisplay(NULL);
	if (!backlight->display) {
		free(backlight);
		return -ENODEV;
	}

	if (!DPMSGetVersion(backlight->display, &major, &minor)) {
		XCloseDisplay(backlight->display);
		free(backlight);
		return -ENOTSUP;
	}

	backlight->dpms = DPMSCapable(backlight->display);
	if (backlight->dpms) {
		if (!DPMSQueryExtension(backlight->display, &dummy, &dummy)) {
			XCloseDisplay(backlight->display);
			free(backlight);
			return -ENOTSUP;
		}
	} else {
		/* Fallback to Screensaver */
		g_debug("   Display does not support blank,"
			" fallback to screen saver");
	}

	*backlightp = backlight;
	return 0;
}

int backlight_free(struct backlight *backlight)
{
	if (!backlight)
		return -EINVAL;

	XCloseDisplay(backlight->display);
	free(backlight);

	return 0;
}

int backlight_enable(struct backlight *backlight, bool enable)
{
	if (!backlight)
		return -EINVAL;

	if (backlight->dpms) {
		DPMSForceLevel(backlight->display, enable ? DPMSModeOn : DPMSModeOff);
		XFlush(backlight->display);
	} else {
		if (enable)
			XResetScreenSaver(backlight->display);
		else
			XActivateScreenSaver(backlight->display);

		XFlush(backlight->display);
	}

	return 0;
}

int backlight_is_enabled(struct backlight *backlight)
{
	g_debug("Querying screensaver states is currently not supported");

	return -ENOSYS;
}

int backlight_set(struct backlight *backlight, unsigned int brightness)
{
	CARD16 level = (brightness > BACKLIGHT_MIN) ? DPMSModeOn : DPMSModeOff;

	if (!backlight)
		return -EINVAL;

	if (backlight->dpms) {
		DPMSForceLevel(backlight->display, level);
		XFlush(backlight->display);
	}

	return 0;
}

int backlight_get(struct backlight *backlight)
{
	CARD16 level = DPMSModeOff;
	BOOL state = False;

	if (!backlight)
		return -EINVAL;

	if (backlight->dpms) {
		if (!DPMSInfo(backlight->display, &level, &state))
			return -ENOTSUP;
	}

	if (level == DPMSModeOn)
		return BACKLIGHT_MAX;

	return 0;
}
