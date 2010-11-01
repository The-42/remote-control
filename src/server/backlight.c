#include <glib/gprintf.h>

#include "remote-control.h"

int32_t medcom_backlight_enable(void *priv, uint32_t flags)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, flags=%x)", __func__, priv, flags);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_backlight_set(void *priv, uint8_t brightness)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, brightness=%02x)", __func__, priv, brightness);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_backlight_get(void *priv, uint8_t *brightness)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, brightness=%p)", __func__, priv, brightness);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}
