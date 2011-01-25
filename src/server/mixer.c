/*
 * Copyright (C) 2010-2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <glib.h>

#include "remote-control-stub.h"
#include "remote-control.h"

int32_t medcom_mixer_set_volume(void *priv, enum medcom_mixer_control control,
		uint8_t volume)
{
	struct mixer *mixer = remote_control_get_mixer(priv);
	return mixer_set_volume(mixer, control, volume);
}

int32_t medcom_mixer_get_volume(void *priv, enum medcom_mixer_control control,
		uint8_t *volumep)
{
	struct mixer *mixer = remote_control_get_mixer(priv);
	unsigned int volume = 0;
	int32_t ret;

	ret = mixer_get_volume(mixer, control, &volume);

	if (volumep)
		*volumep = volume;

	return ret;
}

int32_t medcom_mixer_set_mute(void *priv, enum medcom_mixer_control control, bool mute)
{
	struct mixer *mixer = remote_control_get_mixer(priv);
	return mixer_set_mute(mixer, control, mute);
}

int32_t medcom_mixer_is_muted(void *priv, enum medcom_mixer_control control, bool *mutep)
{
	struct mixer *mixer = remote_control_get_mixer(priv);
	return mixer_is_muted(mixer, control, mutep);
}

int32_t medcom_mixer_set_input_source(void *priv, enum medcom_mixer_input_source source)
{
	struct mixer *mixer = remote_control_get_mixer(priv);
	unsigned short input = MIXER_INPUT_SOURCE_UNKNOWN;

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

	return mixer_set_input_source(mixer, input);
}

int32_t medcom_mixer_get_input_source(void *priv, enum medcom_mixer_input_source *sourcep)
{
	enum mixer_input_source input = MIXER_INPUT_SOURCE_UNKNOWN;
	struct mixer *mixer = remote_control_get_mixer(priv);
	int32_t err;

	err = mixer_get_input_source(mixer, &input);
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
