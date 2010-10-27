#include "remote-control.h"

int32_t medcom_media_player_start(void *priv)
{
	return -ENOSYS;
}

int32_t medcom_media_player_stop(void *priv)
{
	return -ENOSYS;
}

int32_t medcom_media_player_is_running(void *priv, bool *running)
{
	return -ENOSYS;
}

int32_t medcom_media_player_set_stream(void *priv, const char *url)
{
	return -ENOSYS;
}

int32_t medcom_media_player_get_stream(void *priv, char **url)
{
	return -ENOSYS;
}

int32_t medcom_media_player_set_output_window(void *priv, uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
	return -ENOSYS;
}
