#include <glib.h>

#include "remote-control-stub.h"
#include "remote-control.h"

int32_t medcom_mixer_set_volume(void *priv, enum medcom_mixer_control control,
		uint8_t volume)
{
	struct remote_control *rc = priv;
	int32_t ret;

	g_debug("> %s(priv=%p, control=%d, volume=%d)", __func__, priv, control,
			volume);

	ret = mixer_set_volume(rc->mixer, control, volume);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_mixer_get_volume(void *priv, enum medcom_mixer_control control,
		uint8_t *volumep)
{
	struct remote_control *rc = priv;
	int32_t ret;

	g_debug("> %s(priv=%p, control=%d, volumep=%p)", __func__, priv, control,
			volumep);

	ret = mixer_get_volume(rc->mixer, control, volumep);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_mixer_set_mute(void *priv, enum medcom_mixer_control control, bool mute)
{
	struct remote_control *rc = priv;
	int32_t ret;

	g_debug("> %s(priv=%p, control=%d, mute=%d)", __func__, priv, control, mute);

	ret = mixer_set_mute(rc->mixer, control, mute);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_mixer_is_muted(void *priv, enum medcom_mixer_control control, bool *mutep)
{
	struct remote_control *rc = priv;
	int32_t ret;

	g_debug("> %s(priv=%p, control=%d, mutep=%p)", __func__, priv, control, mutep);

	ret = mixer_is_muted(rc->mixer, control, mutep);

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_mixer_set_capture_source(void *priv, enum medcom_mixer_control control)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, control=%d)", __func__, priv, control);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_mixer_get_capture_source(void *priv, enum medcom_mixer_control *control)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, control=%p)", __func__, priv, control);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}
