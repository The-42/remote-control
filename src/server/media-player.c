#include <glib.h>

#include "remote-control-stub.h"
#include "remote-control.h"

int32_t medcom_media_player_start(void *priv)
{
	struct remote_control *rc = priv;
	int32_t ret;

	g_debug("> %s(priv=%p)", __func__, priv);

	ret = media_player_play(rc->player);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_media_player_stop(void *priv)
{
	struct remote_control *rc = priv;
	int32_t ret;

	g_debug("> %s(priv=%p)", __func__, priv);

	ret = media_player_stop(rc->player);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_media_player_is_running(void *priv, bool *running)
{
	enum media_player_state state = MEDIA_PLAYER_STOPPED;
	struct remote_control *rc = priv;
	int32_t ret = 0;
	int err;

	g_debug("> %s(priv=%p, running=%p)", __func__, priv, running);

	err = media_player_get_state(rc->player, &state);
	if (err < 0) {
		ret = err;
		goto out;
	}

	switch (state) {
	case MEDIA_PLAYER_PLAYING:
		*running = true;
		break;

	default:
		*running = false;
		break;
	}

out:
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_media_player_set_stream(void *priv, const char *url)
{
	struct remote_control *rc = priv;
	int32_t ret;

	g_debug("> %s(priv=%p, url=%s)", __func__, priv, url);

	ret = media_player_set_uri(rc->player, url);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_media_player_get_stream(void *priv, char **url)
{
	struct remote_control *rc = priv;

	return media_player_get_uri(rc->player, url);
}

int32_t medcom_media_player_set_output_window(void *priv, uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
	struct remote_control *rc = priv;
	int32_t ret;

	g_debug("> %s(priv=%p, x=%u, y=%u, width=%u, height=%u)", __func__,
			priv, x, y, width, height);

	ret = media_player_set_output_window(rc->player, x, y, width, height);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}
