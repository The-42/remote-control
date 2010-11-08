#include <glib.h>

#include "remote-control.h"

int32_t medcom_media_player_start(void *priv)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret;
	int err;

	g_debug("> %s(priv=%p)", __func__, priv);

	err = medcom_media_player_start_stub(rpc, &ret);
	if (err < 0) {
		ret = err;
		goto out;
	}

out:
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_media_player_stop(void *priv)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret;
	int err;

	g_debug("> %s(priv=%p)", __func__, priv);

	err = medcom_media_player_stop_stub(rpc, &ret);
	if (err < 0) {
		ret = err;
		goto out;
	}

out:
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_media_player_get_stream(void *priv, char **url)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret;
	int err;

	g_debug("> %s(priv=%p, url=%p)", __func__, priv, url);

	err = medcom_media_player_get_stream_stub(rpc, &ret, url);
	if (err < 0) {
		ret = err;
		goto out;
	}

out:
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_media_player_set_stream(void *priv, const char *url)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret;
	int err;

	g_debug("> %s(priv=%p, url=%s)", __func__, priv, url);

	err = medcom_media_player_set_stream_stub(rpc, &ret, url);
	if (err < 0) {
		ret = err;
		goto out;
	}

out:
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_media_player_set_output_window(void *priv, uint16_t x, uint16_t y,
		uint16_t width, uint16_t height)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret;
	int err;

	g_debug("> %s(priv=%p, x=%u, y=%u, width=%u, height=%u)", __func__,
			priv, x, y, width, height);

	err = medcom_media_player_set_output_window_stub(rpc, &ret, x, y,
			width, height);
	if (err < 0) {
		ret = err;
		goto out;
	}

out:
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}
