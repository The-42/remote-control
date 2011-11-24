/*
 * src/server/audio-alsa.c (ALSA UCM BACKEND)
 *
 * Authors: Soeren Grunewald <soeren.grunewald@avionic-design.de>
 *
 * Copyright (C) 2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "audio-backend.h"

#include <errno.h>

#include <alsa/asoundlib.h>
#include <alsa/use-case.h>

struct audio_context {
	snd_use_case_mgr_t *uc_mgr;
	enum RPC_TYPE(audio_state) state;
};

static void alsa_ucm_dump_verb_devices(struct audio_context *audio, const char *verb)
{
	char identifier[64];
	const char **list = NULL;
	int ret;
	int i;

	snprintf(identifier, sizeof(identifier), "_devices/%s", verb);

	ret = snd_use_case_get_list(audio->uc_mgr, identifier, &list);
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

static void alsa_ucm_dump_verbs(struct audio_context *audio)
{
	const char *current = NULL;
	const char **list = NULL;
	int ret;
	int i;

	ret = snd_use_case_verb_list(audio->uc_mgr, &list);
	if (ret < 0) {
		g_warning("%s: no list found", __func__);
		return;
	}

	g_print("dumping alsa use-case-manager %p verbs:\n", audio->uc_mgr);
	for (i = 0; i < ret; i++) {
		g_print("   * [%s]\n", list[i]);
		alsa_ucm_dump_verb_devices(audio, list[i]);
	}

	ret = snd_use_case_free_list(list, ret);
	if (ret < 0)
		g_warning("%s: failed to free list", __func__);


	ret = snd_use_case_get(audio->uc_mgr, "_verb", &current);
	if (ret < 0)
		g_warning("%s: unable to get current state:", __func__);
	else
		g_print("current state is: %s\n", current);

	if (current)
		free((char *)current);
}

struct ucm_rpc_state_map {
	enum RPC_TYPE(audio_state) state;
	const char *verb;
	const char *dev;
};

static const struct ucm_rpc_state_map map[] = {
	{ RPC_MACRO(AUDIO_STATE_UNKNOWN),
		"Unknown", SND_USE_CASE_DEV_NONE },
	{ RPC_MACRO(AUDIO_STATE_INACTIVE),
		SND_USE_CASE_VERB_INACTIVE, SND_USE_CASE_DEV_NONE },
	{ RPC_MACRO(AUDIO_STATE_HIFI_PLAYBACK_SPEAKER),
		SND_USE_CASE_VERB_HIFI, SND_USE_CASE_DEV_SPEAKER },
	{ RPC_MACRO(AUDIO_STATE_HIFI_PLAYBACK_HEADSET),
		SND_USE_CASE_VERB_HIFI, SND_USE_CASE_DEV_HEADPHONES },
	{ RPC_MACRO(AUDIO_STATE_VOICECALL_HANDSET),
		SND_USE_CASE_VERB_VOICECALL, SND_USE_CASE_DEV_HANDSET },
	{ RPC_MACRO(AUDIO_STATE_VOICECALL_HEADSET),
		SND_USE_CASE_VERB_VOICECALL, SND_USE_CASE_DEV_HEADSET },
	{ RPC_MACRO(AUDIO_STATE_VOICECALL_SPEAKER),
		SND_USE_CASE_VERB_VOICECALL, SND_USE_CASE_DEV_SPEAKER },
	{ RPC_MACRO(AUDIO_STATE_VOICECALL_IP_HANDSET),
		SND_USE_CASE_VERB_IP_VOICECALL, SND_USE_CASE_DEV_HANDSET },
	{ RPC_MACRO(AUDIO_STATE_VOICECALL_IP_HEADSET),
		SND_USE_CASE_VERB_IP_VOICECALL, SND_USE_CASE_DEV_HEADSET },
	{ RPC_MACRO(AUDIO_STATE_VOICECALL_IP_SPEAKER),
		SND_USE_CASE_VERB_IP_VOICECALL, SND_USE_CASE_DEV_SPEAKER },
};

static const char* alsa_ucm_state_to_verb(enum RPC_TYPE(audio_state) state)
{
	int i;

	/* skip unknown state */
	for (i = 1; i < G_N_ELEMENTS(map); i++) {
		if (map[i].state == state) {
			return map[i].verb;
		}
	}

	return NULL;
}

static const char* alsa_ucm_state_to_device(enum RPC_TYPE(audio_state) state)
{
	int i;

	/* skip unknown state */
	for (i = 1; i < G_N_ELEMENTS(map); i++) {
		if (map[i].state == state) {
			return map[i].dev;
		}
	}

	return NULL;
}
#if 0
static enum RPC_TYPE(audio_state) alsa_ucm_case_to_state(const char *verb,
                                                         const char *dev)
{
	int i;

	for (i = 0; i < G_N_ELEMENTS(map); i++) {
		if ((strcmp(map[i].verb, verb) == 0) &&
		    (strcmp(map[i].dev, dev) == 0)) {
			return map[i].state;
		}
	}

	return RPC_MACRO(AUDIO_STATE_UNKNOWN);
}
#endif

static int32_t alsa_ucm_create(struct audio_context **audiop)
{
	static const char card_name[] = "tegrawm8903";
	struct audio_context *audio;
	int err;

	audio = g_new0(struct audio_context, 1);
	if (!audio)
		return -ENOMEM;

	err = snd_use_case_mgr_open(&audio->uc_mgr, card_name);
	if (err < 0) {
		g_warning("%s: failed to open use-case-manager: %d", __func__, err);
		g_free(audio);
		return -ENODEV;
	}

	err = snd_use_case_mgr_reload(audio->uc_mgr);
	if (err < 0) {
		g_warning("%s: failed to reload: %d", __func__, err);
	}

	err = snd_use_case_mgr_reset(audio->uc_mgr);
	if (err < 0) {
		g_warning("%s: failed to reset: %d", __func__, err);
	}

	if (0)
		alsa_ucm_dump_verbs(audio);

	*audiop = audio;
	g_debug("< %s()", __func__);
	return 0;
}

static int32_t alsa_ucm_destroy(struct audio_context *audio)
{
	int err;

	if (!audio)
		return -EINVAL;

	err = snd_use_case_mgr_reset(audio->uc_mgr);
	if (err < 0)
		g_warning("failed to reset use-case-manager: %d", err);

	err = snd_use_case_mgr_close(audio->uc_mgr);
	if (err < 0)
		g_critical("failed to close use-case-manager: %d", err);

	g_free(audio);
	audio = NULL;

	return 0;
}

static int32_t alsa_ucm_get_state(struct audio_context *audio,
                                  enum RPC_TYPE(audio_state) *statep)
{
#if 0
	const char *verb = NULL;
	const char *dev = NULL;
#endif
	int err = 0;

	if (!audio || !statep)
		return -EINVAL;
#if 0
	/*
	 * FIXME: receiving a list from ucm is possible, but as soon as
	 *        you call free on the list ucm will crash on the next call
	 */
	g_debug("   try to get current verb...");
	err = snd_use_case_get(audio->uc_mgr, "_verb", &verb);
	if (err < 0) {
		g_warning("%s: snd_use_case_get(mgr=%p, id=_verb, val=%p)",
			__func__, audio->uc_mgr, verb);
		goto out;
	}

	g_debug("   try to get current device status...");
	err = snd_use_case_get(audio->uc_mgr, "_devstatus", &dev);
	if (err < 0) {
		g_warning("%s: snd_use_case_get(mgr=%p, id=_verb, val=%p)",
			__func__, audio->uc_mgr, verb);
		goto out;
	}

	g_debug("   converting to state....");
	*statep = alsa_ucm_case_to_state(verb, dev);
	err = 0;

out:
	g_debug("   cleaning up");
	if (dev)
		free((char*)dev);
	if (verb)
		free((char*)verb);
#else
	*statep = audio->state;
#endif
	return err;
}

static int32_t alsa_ucm_set_state(struct audio_context *audio,
                                  enum RPC_TYPE(audio_state) state,
                                  bool force)
{
#if 0
	enum RPC_TYPE(audio_state) current_state;
	const char *current_verb;
	const char *current_dev;
#endif
	const char *new_verb;
	const char *new_dev;
	int32_t err;

	g_debug("> %s(audio=%p, state=%u, force=%s)", __func__, audio,
		state, force ? "true" : "false");

	g_debug("   converting state to verb...");
	new_verb = alsa_ucm_state_to_verb(state);
	new_dev = alsa_ucm_state_to_device(state);
	if (!new_verb || !new_dev) {
		g_warning("%s: no match for state=%d found", __func__, state);
		return -EINVAL;
	}

	/* skip test if forced */
	if (force)
		goto enable_verb;
#if 0
	g_debug("   getting current state...");
	/* now get the current state so we can compare and disable devices
	 * if needed */
	err = alsa_ucm_get_state(audio, &current_state);
	if (err < 0) {
		g_warning("%s: unable to get current state", __func__);
		goto enable_verb;
	}

	/* if equal nothing to to */
	if (state == current_state)
		return 0;

	g_debug("   converting state to verb...");
	current_verb = alsa_ucm_state_to_verb(current_state);
	current_dev = alsa_ucm_state_to_device(current_state);
	if (!current_verb || !current_dev) {
		g_warning("%s: no match for state=%d found", __func__, state);
		goto enable_verb;
	}

	g_debug("   current state is %s at device %s", current_verb, current_dev);
	if (strcmp(current_verb, new_verb) == 0) {
		if (strcmp(current_dev, new_dev) == 0) {
			g_debug("   disabling device %s", current_dev);
			err = snd_use_case_set(audio->uc_mgr, "_disdev", current_dev);
			if (err < 0) {
				g_warning("%s: snd_use_case_set(mgr=%p, id=_disdev, val=%s)",
					__func__, audio->uc_mgr, current_dev);
			}
		} else {
			goto enable_dev;
		}
	} else {
		g_debug("state not equal");
	}
#endif
enable_verb:
	g_debug("   setting state to %s...", new_verb);
	g_debug("      snd_use_case_set(mgrp, _verb, %s)", new_verb);
	err = snd_use_case_set(audio->uc_mgr, "_verb", new_verb);
	if (err < 0) {
		g_warning("%s: snd_use_case_set(mgr=%p, id=_verb, val=%s)",
			__func__, audio->uc_mgr, new_verb);
		return err;
	}

/*enable_dev:*/
	g_debug("   enabling device %s...", new_dev);
	g_debug("      snd_use_case_set(mgr, _enadev, %s)", new_dev);
	err = snd_use_case_set(audio->uc_mgr, "_enadev", new_dev);
	if (err < 0) {
		g_warning("%s: snd_use_case_set(mgr=%p, id=_enadev, val=%s) -> %d",
			__func__, audio->uc_mgr, new_dev, err);
		return err;
	}

	audio->state = state;
	return 0;
}

static int set_volume(const char *control, long volume)
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

static int32_t alsa_ucm_set_volume(struct audio_context *audio, uint8_t volume)
{
	int32_t ret = -ENOSYS;

	switch (audio->state) {
	case RPC_MACRO(AUDIO_STATE_HIFI_PLAYBACK_SPEAKER):
	case RPC_MACRO(AUDIO_STATE_VOICECALL_SPEAKER):
	case RPC_MACRO(AUDIO_STATE_VOICECALL_IP_SPEAKER):
		ret = set_volume("Speaker", volume);
		break;
	case RPC_MACRO(AUDIO_STATE_VOICECALL_HANDSET):
	case RPC_MACRO(AUDIO_STATE_VOICECALL_IP_HANDSET):
		ret = set_volume("Line Out", volume);
		break;
	case RPC_MACRO(AUDIO_STATE_HIFI_PLAYBACK_HEADSET):
	case RPC_MACRO(AUDIO_STATE_VOICECALL_HEADSET):
	case RPC_MACRO(AUDIO_STATE_VOICECALL_IP_HEADSET):
		ret = set_volume("Headphone", volume);
		break;
	default:
		/* ignore */
		break;
	}

	return ret;
}

static int get_volume(const char *control, long *volumep)
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

static int32_t alsa_ucm_get_volume(struct audio_context *audio, uint8_t *volume)
{
	int32_t ret = -ENOSYS;
	long vol = 0;

	switch (audio->state) {
	case RPC_MACRO(AUDIO_STATE_HIFI_PLAYBACK_SPEAKER):
	case RPC_MACRO(AUDIO_STATE_VOICECALL_SPEAKER):
	case RPC_MACRO(AUDIO_STATE_VOICECALL_IP_SPEAKER):
		ret = get_volume("Speaker", &vol);
		break;
	case RPC_MACRO(AUDIO_STATE_VOICECALL_HANDSET):
	case RPC_MACRO(AUDIO_STATE_VOICECALL_IP_HANDSET):
		ret = get_volume("Line Out", &vol);
		break;
	case RPC_MACRO(AUDIO_STATE_HIFI_PLAYBACK_HEADSET):
	case RPC_MACRO(AUDIO_STATE_VOICECALL_HEADSET):
	case RPC_MACRO(AUDIO_STATE_VOICECALL_IP_HEADSET):
		ret = get_volume("Headphone", &vol);
		break;
	default:
		/* ignore */
		break;
	}

	*volume = vol;
	return ret;
}

static int32_t alsa_ucm_enable_speakers(struct audio_context *audio, bool enable)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(audio=%p, enable=%d)", __func__, audio, enable);

	if (!audio)
		return -EINVAL;

	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

static int32_t alsa_ucm_speakers_enabled(struct audio_context *audio, bool *enable)
{
	if (!audio || !enable)
		return -EINVAL;

	return 0;
}

static struct audio_impl ucm_impl = {
	.create           = alsa_ucm_create,
	.destroy          = alsa_ucm_destroy,
	.set_state        = alsa_ucm_set_state,
	.get_state        = alsa_ucm_get_state,
	.set_volume       = alsa_ucm_set_volume,
	.get_volume       = alsa_ucm_get_volume,
	.enable_speakers  = alsa_ucm_enable_speakers,
	.speakers_enabled = alsa_ucm_speakers_enabled
};


/*
 * Here comes the public stuff...
 */

struct audio_impl *find_audio_impl()
{
	/* FIXME: for now we have only one implementation */
	if (1)
		return &ucm_impl;
	return NULL;
}

struct audio {
	struct audio_impl *impl;
	struct audio_context *ctx;
};

struct audio_context* audio_get_context(struct audio *audio)
{
	return audio ? audio->ctx : NULL;
}

struct audio_impl* audio_get_impl(struct audio *audio)
{
	return audio ? audio->impl : NULL;
}

int audio_create(struct audio **audiop)
{
	struct audio *audio;
	int ret = 0;

	audio = g_new0(struct audio, 1);
	if (!audio)
		return -ENOMEM;

	audio->impl = find_audio_impl();
	if (audio->impl && audio->impl->create) {
		ret = audio->impl->create(&audio->ctx);
		if (ret < 0)
			g_warning("%s: failed to create context", __func__);
	}
	*audiop = audio;

	return ret;
}

int audio_free(struct audio *audio)
{
	int err = 0;

	if (!audio)
		return -EINVAL;

	audio->impl = find_audio_impl();
	if (audio->impl && audio->impl->destroy) {
		err = audio->impl->destroy(audio->ctx);
		if (err < 0)
			g_warning("%s: failed to destroy: %d", __func__, err);
	}
	g_free(audio);
	audio = NULL;

	return err;
}