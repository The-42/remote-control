#include <X11/Xlib.h>
#include <X11/extensions/dpms.h>

#include <glib.h>

#include "remote-control-stub.h"
#include "remote-control.h"

struct backlight {
	Display *display;
};

int backlight_create(struct backlight **backlightp)
{
	struct backlight *backlight;
	int dummy = 0;

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

	if (!DPMSQueryExtension(backlight->display, &dummy, &dummy)) {
		XCloseDisplay(backlight->display);
		free(backlight);
		return -ENOTSUP;
	}

	*backlightp = backlight;
	return 0;
}

int backlight_free(struct backlight *backlight)
{
	g_debug("> %s(backlight=%p)", __func__, backlight);

	if (!backlight)
		return -EINVAL;

	XCloseDisplay(backlight->display);
	free(backlight);

	g_debug("< %s()", __func__);
	return 0;
}

int backlight_enable(struct backlight *backlight, bool enable)
{
	CARD16 level = enable ? DPMSModeOn : DPMSModeOff;

	g_debug("> %s(backlight=%p, enable=%s)", __func__, backlight, enable ? "true" : "false");

	if (!backlight)
		return -EINVAL;

	DPMSForceLevel(backlight->display, level);
	XFlush(backlight->display);

	g_debug("< %s()", __func__);
	return 0;
}

int backlight_set(struct backlight *backlight, unsigned int brightness)
{
	CARD16 level = (brightness > BACKLIGHT_MIN) ? DPMSModeOn : DPMSModeOff;

	g_debug("> %s(backlight=%p, brightness=%u)", __func__, backlight, brightness);

	if (!backlight)
		return -EINVAL;

	DPMSForceLevel(backlight->display, level);
	XFlush(backlight->display);

	g_debug("< %s()", __func__);
	return 0;
}

int backlight_get(struct backlight *backlight)
{
	CARD16 level = DPMSModeOff;
	BOOL state = False;

	g_debug("> %s(backlight=%p)", __func__, backlight);

	if (!backlight)
		return -EINVAL;

	if (!DPMSInfo(backlight->display, &level, &state))
		return -ENOTSUP;

	if (level == DPMSModeOn)
		return BACKLIGHT_MAX;

	g_debug("< %s()", __func__);
	return 0;
}
