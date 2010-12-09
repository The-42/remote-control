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
	unsigned int volume = 0;
	int32_t ret;

	g_debug("> %s(priv=%p, control=%d, volumep=%p)", __func__, priv, control,
			volumep);

	ret = mixer_get_volume(rc->mixer, control, &volume);

	if (volumep)
		*volumep = volume;

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

int32_t medcom_mixer_set_input_source(void *priv, enum medcom_mixer_input_source source)
{
	unsigned short input = MIXER_INPUT_SOURCE_UNKNOWN;
	struct remote_control *rc = priv;

	switch (source) {
	case MEDCOM_MIXER_INPUT_SOURCE_HEADSET:
		input = MIXER_INPUT_SOURCE_HEADSET;
		break;

	case MEDCOM_MIXER_INPUT_SOURCE_HANDSET:
		input = MIXER_INPUT_SOURCE_HANDSET;
		break;

	case MEDCOM_MIXER_INPUT_SOURCE_LINE:
		input = MIXER_INPUT_SOURCE_LINE;
		break;

	default:
		break;
	}

	return mixer_set_input_source(rc->mixer, input);
}

int32_t medcom_mixer_get_input_source(void *priv, enum medcom_mixer_input_source *sourcep)
{
	enum mixer_input_source input = MIXER_INPUT_SOURCE_UNKNOWN;
	struct remote_control *rc = priv;
	int32_t err;

	err = mixer_get_input_source(rc->mixer, &input);
	if (err < 0)
		return err;

	switch (input) {
	case MIXER_INPUT_SOURCE_HEADSET:
		*sourcep = MEDCOM_MIXER_INPUT_SOURCE_HEADSET;
		break;

	case MIXER_INPUT_SOURCE_HANDSET:
		*sourcep = MEDCOM_MIXER_INPUT_SOURCE_HANDSET;
		break;

	case MIXER_INPUT_SOURCE_LINE:
		*sourcep = MEDCOM_MIXER_INPUT_SOURCE_LINE;
		break;

	default:
		*sourcep = MEDCOM_MIXER_INPUT_SOURCE_UNKNOWN;
		break;
	}

	return 0;
}
