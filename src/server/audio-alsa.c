/*
 * Copyright (C) 2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <asoundlib.h>
#include <use-case.h>

#include "remote-control-stub.h"
#include "remote-control.h"

#define AUDIO_ALSA_DEBUG 0

struct audio {
	snd_use_case_mgr_t *ucm;
	enum audio_state state;
};

static void alsa_ucm_dump_verb_devices(struct audio *audio, const char *verb)
{
	char identifier[64];
	const char **list = NULL;
	int ret;
	int i;

	snprintf(identifier, sizeof(identifier), "_devices/%s", verb);

	ret = snd_use_case_get_list(audio->ucm, identifier, &list);
	if (ret < 0) {
		g_print("      * nothing found for: [%s]\n", identifier);
		return;
	}

	for (i = 0; i < ret; i++)
		g_print("      * [%s]\n", list[i]);

	ret = snd_use_case_free_list(list, ret);
	if (ret < 0)
		g_warning("%s: failed to free list", __func__);
}

static void alsa_ucm_dump_verbs(struct audio *audio)
{
	const char *current = NULL;
	const char **list = NULL;
	int ret;
	int i;

	ret = snd_use_case_verb_list(audio->ucm, &list);
	if (ret < 0) {
		g_warning("%s: no list found", __func__);
		return;
	}

	g_print("dumping alsa use-case-manager %p verbs:\n", audio->ucm);
	for (i = 0; i < ret; i++) {
		g_print("   * [%s]\n", list[i]);
		alsa_ucm_dump_verb_devices(audio, list[i]);
	}

	ret = snd_use_case_free_list(list, ret);
	if (ret < 0)
		g_warning("%s: failed to free list", __func__);


	ret = snd_use_case_get(audio->ucm, "_verb", &current);
	if (ret < 0)
		g_warning("%s: unable to get current state:", __func__);
	else
		g_print("current state is: %s\n", current);

	if (current)
		free((char *)current);
}

struct ucm_state {
	const char *verb;
	const char *device;
};

static const struct ucm_state ucm_states[] = {
	[AUDIO_STATE_INACTIVE] = {
		.verb = SND_USE_CASE_VERB_INACTIVE,
		.device = SND_USE_CASE_DEV_NONE,
	},
	[AUDIO_STATE_HIFI_PLAYBACK_SPEAKER] = {
		.verb = SND_USE_CASE_VERB_HIFI,
		.device = SND_USE_CASE_DEV_SPEAKER,
	},
	[AUDIO_STATE_HIFI_PLAYBACK_HEADSET] = {
		.verb = SND_USE_CASE_VERB_HIFI,
		.device = SND_USE_CASE_DEV_HEADPHONES,
	},
	[AUDIO_STATE_VOICECALL_HANDSET] = {
		.verb = SND_USE_CASE_VERB_VOICECALL,
		.device = SND_USE_CASE_DEV_HANDSET,
	},
	[AUDIO_STATE_VOICECALL_HEADSET] = {
		.verb = SND_USE_CASE_VERB_VOICECALL,
		.device = SND_USE_CASE_DEV_HEADSET,
	},
	[AUDIO_STATE_VOICECALL_SPEAKER] = {
		.verb = SND_USE_CASE_VERB_VOICECALL,
		.device = SND_USE_CASE_DEV_SPEAKER,
	},
	[AUDIO_STATE_VOICECALL_IP_HANDSET] = {
		.verb = SND_USE_CASE_VERB_IP_VOICECALL,
		.device = SND_USE_CASE_DEV_HANDSET,
	},
	[AUDIO_STATE_VOICECALL_IP_HEADSET] = {
		.verb = SND_USE_CASE_VERB_IP_VOICECALL,
		.device = SND_USE_CASE_DEV_HEADSET,
	},
	[AUDIO_STATE_VOICECALL_IP_SPEAKER] = {
		.verb = SND_USE_CASE_VERB_IP_VOICECALL,
		.device = SND_USE_CASE_DEV_SPEAKER,
	},
};

static int control_set_volume(const char *control, long volume)
{
	const char *card = "default";
	snd_mixer_selem_id_t *sid;
	snd_mixer_elem_t* elem;
	snd_mixer_t *handle;
	int err;

	/*
	 * FIXME: this is a dirty hack for medica.
	 */
	err = snd_mixer_open(&handle, 0);
	err = snd_mixer_attach(handle, card);
	err = snd_mixer_selem_register(handle, NULL, NULL);
	err = snd_mixer_load(handle);

	snd_mixer_selem_id_alloca(&sid);
	snd_mixer_selem_id_set_index(sid, 0);
	snd_mixer_selem_id_set_name(sid, control);

	elem = snd_mixer_find_selem(handle, sid);
	if (elem) {
		long min, max;
		snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
		volume = ((volume * (max - min)) / 255) + min;
		err = snd_mixer_selem_set_playback_volume_all(elem, volume);
	}

	snd_mixer_close(handle);
	return err;
}

static int control_get_volume(const char *control, long *volumep)
{
	const char *card = "default";
	snd_mixer_selem_id_t *sid;
	snd_mixer_elem_t* elem;
	snd_mixer_t *handle;
	long volume;
	int err;

	/*
	 * FIXME: this is a dirty hack for medica.
	 */
	err = snd_mixer_open(&handle, 0);
	err = snd_mixer_attach(handle, card);
	err = snd_mixer_selem_register(handle, NULL, NULL);
	err = snd_mixer_load(handle);

	snd_mixer_selem_id_alloca(&sid);
	snd_mixer_selem_id_set_index(sid, 0);
	snd_mixer_selem_id_set_name(sid, control);

	elem = snd_mixer_find_selem(handle, sid);
	if (elem) {
		long min, max;
		snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
		/* since we set both, we can query only one side */
		err = snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, &volume);
		volume = ((volume + min) * 255) / (max - min);
		*volumep = volume;
	}

	snd_mixer_close(handle);
	return err;
}

int audio_create(struct audio **audiop)
{
	/* TODO: do not hardcode the card name here! */
	static const char card_name[] = "tegrawm8903";
	struct audio *audio;
	int err;

	audio = g_new0(struct audio, 1);
	if (!audio)
		return -ENOMEM;

	err = snd_use_case_mgr_open(&audio->ucm, card_name);
	if (err < 0) {
		g_warning("audio-alsa-ucm: failed to open use-case-manager: "
				"%s", snd_strerror(err));
		g_free(audio);
		return -ENODEV;
	}

	err = snd_use_case_mgr_reload(audio->ucm);
	if (err < 0) {
		g_warning("audio-alsa-ucm: failed to reload: %s",
				snd_strerror(err));
	}

	err = snd_use_case_mgr_reset(audio->ucm);
	if (err < 0) {
		g_warning("audio-alsa-ucm: failed to reset: %s",
				snd_strerror(err));
	}

	if (AUDIO_ALSA_DEBUG)
		alsa_ucm_dump_verbs(audio);

	*audiop = audio;
	return 0;
}

int audio_free(struct audio *audio)
{
	int err;

	if (!audio)
		return -EINVAL;

	err = snd_use_case_mgr_reset(audio->ucm);
	if (err < 0) {
		g_warning("audio-alsa-ucm: failed to reset UCM: %s",
				snd_strerror(err));
	}

	err = snd_use_case_mgr_close(audio->ucm);
	if (err < 0) {
		g_warning("audio-alsa-ucm: failed to close UCM: %s",
				snd_strerror(err));
	}

	g_free(audio);
	return 0;
}

int audio_set_state(struct audio *audio, enum audio_state state)
{
	const struct ucm_state *s;
	int err;

	if (!audio || (state < 0) || (state >= G_N_ELEMENTS(ucm_states)))
		return -EINVAL;

	s = &ucm_states[state];

	err = snd_use_case_set(audio->ucm, "_verb", s->verb);
	if (err < 0) {
		g_warning("audio-alsa-ucm: failed to set use-case: %s",
				snd_strerror(err));
		return err;
	}

	err = snd_use_case_set(audio->ucm, "_enadev", s->device);
	if (err < 0) {
		g_warning("audio-alsa-ucm: failed to enable device: %s",
				snd_strerror(err));
		return err;
	}

	audio->state = state;
	return 0;
}

int audio_get_state(struct audio *audio, enum audio_state *statep)
{
	if (!audio || !statep)
		return -EINVAL;

	*statep = audio->state;
	return 0;
}

int audio_set_volume(struct audio *audio, uint8_t volume)
{
	int ret;

	/*
	 * TODO: Find a way to obtain which volume control to use from the
	 *       ALSA UCM.
	 */

	switch (audio->state) {
	case AUDIO_STATE_HIFI_PLAYBACK_SPEAKER:
	case AUDIO_STATE_VOICECALL_SPEAKER:
	case AUDIO_STATE_VOICECALL_IP_SPEAKER:
		ret = control_set_volume("Speaker", volume);
		break;

	case AUDIO_STATE_VOICECALL_HANDSET:
	case AUDIO_STATE_VOICECALL_IP_HANDSET:
		ret = control_set_volume("Line Out", volume);
		break;

	case AUDIO_STATE_HIFI_PLAYBACK_HEADSET:
	case AUDIO_STATE_VOICECALL_HEADSET:
	case AUDIO_STATE_VOICECALL_IP_HEADSET:
		ret = control_set_volume("Headphone", volume);
		break;

	default:
		ret = -EBADFD;
		break;
	}

	return ret;
}

int audio_get_volume(struct audio *audio, uint8_t *volumep)
{
	long volume = 0;
	int ret;

	/*
	 * TODO: Find a way to obtain which volume control to use from the
	 *       ALSA UCM.
	 */

	switch (audio->state) {
	case AUDIO_STATE_HIFI_PLAYBACK_SPEAKER:
	case AUDIO_STATE_VOICECALL_SPEAKER:
	case AUDIO_STATE_VOICECALL_IP_SPEAKER:
		ret = control_get_volume("Speaker", &volume);
		break;

	case AUDIO_STATE_VOICECALL_HANDSET:
	case AUDIO_STATE_VOICECALL_IP_HANDSET:
		ret = control_get_volume("Line Out", &volume);
		break;

	case AUDIO_STATE_HIFI_PLAYBACK_HEADSET:
	case AUDIO_STATE_VOICECALL_HEADSET:
	case AUDIO_STATE_VOICECALL_IP_HEADSET:
		ret = control_get_volume("Headphone", &volume);
		break;

	default:
		ret = -EBADFD;
		break;
	}

	*volumep = volume;
	return ret;
}

int audio_set_speakers_enable(struct audio *audio, bool enable)
{
	return -ENOSYS;
}

int audio_get_speakers_enable(struct audio *audio, bool *enablep)
{
	return -ENOSYS;
}
