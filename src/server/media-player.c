#include <glib.h>

#include "remote-control.h"

int32_t medcom_media_player_start(void *priv)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p)", __func__, priv);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_media_player_stop(void *priv)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p)", __func__, priv);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_media_player_is_running(void *priv, bool *running)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, running=%p)", __func__, priv, running);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_media_player_set_stream(void *priv, const char *url)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, url=%s)", __func__, priv, url);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_media_player_get_stream(void *priv, char **url)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, url=%p)", __func__, priv, url);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_media_player_set_output_window(void *priv, uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, x=%u, y=%u, width=%u, height=%u)", __func__,
			priv, x, y, width, height);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}
