#include <glib.h>

#include "remote-control.h"

int32_t medcom_media_player_start(void *priv)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret;
	int err;

	err = medcom_media_player_start_stub(rpc, &ret);
	if (err < 0)
		return err;

	return ret;
}

int32_t medcom_media_player_stop(void *priv)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret;
	int err;

	err = medcom_media_player_stop_stub(rpc, &ret);
	if (err < 0)
		return err;

	return ret;
}

int32_t medcom_media_player_is_running(void *priv, bool *running)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int err;

	err = medcom_media_player_is_running_stub(rpc, &ret, running);
	if (err < 0)
		return err;

	return ret;
}

int32_t medcom_media_player_get_stream(void *priv, char **url)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret;
	int err;

	err = medcom_media_player_get_stream_stub(rpc, &ret, url);
	if (err < 0)
		return err;

	return ret;
}

int32_t medcom_media_player_set_stream(void *priv, const char *url)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret;
	int err;

	err = medcom_media_player_set_stream_stub(rpc, &ret, url);
	if (err < 0)
		return err;

	return ret;
}

int32_t medcom_media_player_set_output_window(void *priv, uint16_t x, uint16_t y,
		uint16_t width, uint16_t height)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret;
	int err;

	err = medcom_media_player_set_output_window_stub(rpc, &ret, x, y,
			width, height);
	if (err < 0)
		return err;

	return ret;
}
