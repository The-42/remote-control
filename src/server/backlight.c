#include <glib.h>

#include "remote-control-stub.h"
#include "remote-control.h"

int32_t medcom_backlight_enable(void *priv, uint32_t flags)
{
	struct remote_control *rc = priv;
	int32_t ret;

	g_debug("> %s(priv=%p, flags=%x)", __func__, priv, flags);

	ret = backlight_enable(rc->backlight, (flags & 0x1) == 0x1);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_backlight_set(void *priv, uint8_t brightness)
{
	struct remote_control *rc = priv;
	int32_t ret;

	g_debug("> %s(priv=%p, brightness=%02x)", __func__, priv, brightness);

	ret = backlight_set(rc->backlight, brightness);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_backlight_get(void *priv, uint8_t *brightness)
{
	struct remote_control *rc = priv;
	int32_t ret;

	g_debug("> %s(priv=%p, brightness=%p)", __func__, priv, brightness);

	ret = backlight_get(rc->backlight);
	if (ret >= 0) {
		*brightness = ret;
		ret = 0;
	}

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}
