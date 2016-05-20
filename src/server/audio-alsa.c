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

#include <alsa/asoundlib.h>
#include <alsa/use-case.h>
#include <glib.h>

#include "remote-control-stub.h"
#include "remote-control.h"

#include "find-device.h"

#define AUDIO_ALSA_DEBUG 0
#define AUDIO_USE_DEVICE_PREFIX "BurrBrown from Texas Instruments "
#define AUDIO_USB_DEVICE_NAME "USB AUDIO  CODEC"
#define AUDIO_USB_DEVICE_FULL_NAME AUDIO_USE_DEVICE_PREFIX AUDIO_USB_DEVICE_NAME

struct soundcard {
	gchar *name;
	int index;
	snd_mixer_t *mixer;
};

struct audio {
	struct remote_control *rc;
	snd_use_case_mgr_t *ucm;
	enum audio_state state;

	gint index;
	GList *cards;

	gboolean usb_handset;
};

int audio_get_state(struct audio *audio, enum audio_state *statep);

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
	[AUDIO_STATE_LINEIN_SPEAKER] = {
		.verb = SND_USE_CASE_VERB_ANALOG_RADIO,
		.device = SND_USE_CASE_DEV_SPEAKER,
	},
	[AUDIO_STATE_LINEIN_HEADSET] = {
		.verb = SND_USE_CASE_VERB_ANALOG_RADIO,
		.device = SND_USE_CASE_DEV_HEADSET,
	},
};

static const char *audio_state_to_control(struct audio *audio)
{
	const char *control = NULL;

	switch (audio->state) {
	case AUDIO_STATE_HIFI_PLAYBACK_SPEAKER:
	case AUDIO_STATE_VOICECALL_SPEAKER:
	case AUDIO_STATE_VOICECALL_IP_SPEAKER:
	case AUDIO_STATE_LINEIN_SPEAKER:
		control = "Speaker";
		break;

	case AUDIO_STATE_VOICECALL_HANDSET:
	case AUDIO_STATE_VOICECALL_IP_HANDSET:
		control = "Line Out";
		break;

	case AUDIO_STATE_HIFI_PLAYBACK_HEADSET:
	case AUDIO_STATE_VOICECALL_HEADSET:
	case AUDIO_STATE_VOICECALL_IP_HEADSET:
	case AUDIO_STATE_LINEIN_HEADSET:
		control = "Headphone";
		break;

	default:
		control = "Master";
	}
	return control;
}

static void soundcard_mixer_close(struct soundcard *card);
static int soundcard_mixer_open(struct soundcard *card);

static inline snd_mixer_elem_t* soundcard_get_control(struct soundcard *card,
						      const char *control)
{
	snd_mixer_selem_id_t *sid;

	snd_mixer_selem_id_alloca(&sid);
	snd_mixer_selem_id_set_index(sid, 0);
	snd_mixer_selem_id_set_name(sid, control);

	return snd_mixer_find_selem(card->mixer, sid);
}

static int soundcard_control_set_volume(struct soundcard *card,
					const char *control, long volume)
{
	long min = 0, max = 0, vol;
	snd_mixer_elem_t* elem;
	int err;

	err = soundcard_mixer_open(card);
	if (err < 0)
		return err;

	elem = soundcard_get_control(card, control);
	if (!elem) {
		soundcard_mixer_close(card);
		return -ENODEV;
	}

	err = snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
	if (err < 0)
		return err;

	vol = ((volume * (max - min)) / 255) + min;

	err = snd_mixer_selem_set_playback_volume_all(elem, vol);
	if (err < 0)
		return err;

	snd_mixer_selem_set_playback_switch_all(elem, vol > 0);

	soundcard_mixer_close(card);
	return 0;
}

static int soundcard_control_get_volume(struct soundcard *card,
					const char *control, long *volumep)
{
	snd_mixer_elem_t* elem;
	long min, max;
	long volume;
	int err;

	err = soundcard_mixer_open(card);
	if (err < 0)
		return err;

	elem = soundcard_get_control(card, control);
	if (!elem) {
		soundcard_mixer_close(card);
		return -ENODEV;
	}

	snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
	/* since we set both, we can query only one side */
	err = snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, &volume);
	volume = ((volume + min) * 255) / (max - min);
	*volumep = volume;

	soundcard_mixer_close(card);
	return err;
}

static void soundcard_mixer_close(struct soundcard *card)
{
	char device[16];

	if (!card || !card->mixer)
		return;

	snprintf(device, sizeof(device), "hw:%d", card->index);
	snd_mixer_detach(card->mixer, device);
	snd_mixer_close(card->mixer);
	card->mixer = NULL;
}

static int soundcard_mixer_open(struct soundcard *card)
{
	char device[16];
	int err;

	err = snd_mixer_open(&card->mixer, 0);
	if (err < 0)
		goto out;

	snprintf(device, sizeof(device), "hw:%d", card->index);

	err = snd_mixer_attach(card->mixer, device);
	if (err < 0)
		goto close;

	err = snd_mixer_selem_register(card->mixer, NULL, NULL);
	if (err < 0)
		goto detach;

	err = snd_mixer_load(card->mixer);
	if (err < 0)
		goto detach;

	return 0;

detach:
	snd_mixer_detach(card->mixer, device);
close:
	snd_mixer_close(card->mixer);
	card->mixer = NULL;
out:
	return err;
}

static struct soundcard *soundcard_new(const char *name, int index)
{
	struct soundcard *card;

	card = g_new0(struct soundcard, 1);
	if (!card)
		return NULL;

	card->index = index;
	card->name = g_strdup(name);
	return card;
}

static void soundcard_free(struct soundcard *card)
{
	if (!card)
		return;

	soundcard_mixer_close(card);
	card->index = -1;
	g_free(card->name);
	card->name = NULL;
	g_free(card);
}

static gint card_compare_name(gconstpointer a, gconstpointer b)
{
	const struct soundcard *card = a;
	const char *name = b;

	return g_strcmp0(card->name, name);
}

static struct soundcard *audio_get_card_by_name(struct audio *audio,
						const char *name)
{
	struct soundcard *card = NULL;
	GList *node;

	node = g_list_find_custom(audio->cards, name, card_compare_name);
	if (node)
		card = node->data;

	return card;
}

static int audio_find_cards(struct audio *audio)
{
	snd_ctl_card_info_t *info;
	struct soundcard *card;
	int number = -1;
	snd_ctl_t *ctl;
	char dev[16];
	int err;

	snd_ctl_card_info_alloca(&info);

	while (true) {
		err = snd_card_next(&number);
		if (err < 0) {
			g_warning("audio-alsa-ucm: unable to enumerate sound cards");
			break;
		}

		if (number < 0)
			break;

		sprintf(dev, "hw:%d", number);
		err = snd_ctl_open(&ctl, dev, 0);
		if (err < 0) {
			g_debug("audio-alsa-ucm: unable to open %s", dev);
			continue;
		}

		err = snd_ctl_card_info(ctl, info);
		snd_ctl_close(ctl);
		if (err < 0)
			continue;

		card = soundcard_new(snd_ctl_card_info_get_name(info), number);
		if (card) {
			g_debug("audio-alsa-ucm: soundcard %s created (%s)",
				card->name, snd_ctl_card_info_get_driver(info));
			audio->cards = g_list_append(audio->cards, card);
		}
	}

	return g_list_length(audio->cards) > 0 ? 0 : -ENODEV;
}

int audio_create(struct audio **audiop, struct rpc_server *server,
		 GKeyFile *config)
{
	struct soundcard *card;
	struct audio *audio;
	int err, ret;

	audio = g_new0(struct audio, 1);
	if (!audio)
		return -ENOMEM;

	audio->rc = rpc_server_priv(server);

	err = audio_find_cards(audio);
	if (err < 0) {
		g_warning("audio-alsa-ucm: No card found, %d", err);
		g_free(audio);
		return err;
	}

	card = g_list_nth(audio->cards, audio->index)->data;

	err = snd_use_case_mgr_open(&audio->ucm, card->name);
	if (err < 0) {
		g_warning("audio-alsa-ucm: failed to open use-case-manager: "
			  "%s", snd_strerror(err));
		audio_free(audio);
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

	ret = find_input_devices(AUDIO_USB_DEVICE_FULL_NAME, NULL, NULL);
	if (ret > 0) {
		g_debug("audio-alsa-ucm: found usb handset");
		audio->usb_handset = true;
	} else
		audio->usb_handset = false;

	*audiop = audio;
	return 0;
}

int audio_free(struct audio *audio)
{
	int err;

	if (!audio)
		return -EINVAL;

	if (audio->ucm) {
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
	}

	g_list_free_full(audio->cards, (GDestroyNotify)soundcard_free);

	g_free(audio);
	return 0;
}

static void audio_update_voip(struct audio *audio, const struct ucm_state *s)
{
	struct voip *voip = remote_control_get_voip(audio->rc);
	char identifier[128];
	char card[128];
	char *name;

	if (!voip || strcmp(s->verb, SND_USE_CASE_VERB_IP_VOICECALL) ||
			!strcmp(s->device, SND_USE_CASE_DEV_NONE))
		return;

	snprintf(identifier, sizeof(identifier), "PlaybackPCM/%s", s->device);
	if (!snd_use_case_get(audio->ucm, identifier, (const char **)&name) ||
			!snd_use_case_get(audio->ucm, "PlaybackPCM",
					(const char **)&name)) {
		snprintf(card, sizeof(card), "ALSA: %s", name);
		voip_set_playback(voip, card);
		g_free(name);
	}

	snprintf(identifier, sizeof(identifier), "CapturePCM/%s", s->device);
	if (!snd_use_case_get(audio->ucm, identifier, (const char **)&name) ||
			!snd_use_case_get(audio->ucm, "CapturePCM",
					(const char **)&name)) {
		snprintf(card, sizeof(card), "ALSA: %s", name);
		voip_set_capture(voip, card);
		g_free(name);
	}
	snprintf(identifier, sizeof(identifier), "CaptureGain/%s", s->device);
	if (!snd_use_case_get(audio->ucm, identifier, (const char **)&name) ||
			!snd_use_case_get(audio->ucm, "CaptureGain",
					(const char **)&name)) {
		float gain = atof(name);
		if (gain != 0.0)
			voip_set_capture_gain(voip, gain);
		g_free(name);
	}
}

int audio_set_state(struct audio *audio, enum audio_state state)
{
	const struct ucm_state *s;
	enum audio_state prev_state;
	int err;

	if (!audio || (state < 0) || (state >= G_N_ELEMENTS(ucm_states)))
		return -EINVAL;

	s = &ucm_states[state];
	g_debug("audio-alsa-ucm: set state to %d", state);

	/* FIXME: we wan't to change this in favor of a more generic API
	 *        that would allow the frontend to enable and disable devices
	 *        explicitly. */
	/* first disable the previous state */
	if (audio_get_state (audio, &prev_state) == 0 &&
	    strcmp(ucm_states[prev_state].device, SND_USE_CASE_DEV_NONE) != 0) {
		err = snd_use_case_set(audio->ucm, "_disdev",
				       ucm_states[prev_state].device);
		if (err < 0) {
			g_warning("audio-alsa-ucm: failed to disable device %s: %s",
				  ucm_states[prev_state].device, snd_strerror(err));
		}
	}

	g_debug("audio-alsa-ucm: set verb %s", s->verb);
	err = snd_use_case_set(audio->ucm, "_verb", s->verb);
	if (err < 0) {
		g_warning("audio-alsa-ucm: failed to set use-case %s: %s",
			  s->verb, snd_strerror(err));
		return err;
	}

	if (strcmp(s->device, SND_USE_CASE_DEV_NONE) != 0) {
		err = snd_use_case_set(audio->ucm, "_enadev", s->device);
		if (err < 0) {
			g_warning("audio-alsa-ucm: failed to enable device %s: %s",
				  s->device, snd_strerror(err));
			return err;
		}
	}

	audio->state = state;

	audio_update_voip(audio, s);

	return 0;
}

int audio_get_state(struct audio *audio, enum audio_state *statep)
{
	if (!audio || !statep)
		return -EINVAL;

	*statep = audio->state;
	return 0;
}

static gboolean audio_is_handset_selected(struct audio *audio)
{
	if (audio->state == AUDIO_STATE_VOICECALL_HANDSET ||
	    audio->state == AUDIO_STATE_VOICECALL_IP_HANDSET)
		return true;
	return false;
}

int audio_set_volume(struct audio *audio, uint8_t volume)
{
	struct soundcard *card;
	const char *control;

	control = audio_state_to_control(audio);
	if (!control)
		return -EBADFD;

	card = g_list_nth(audio->cards, audio->index)->data;
	if (!card)
		return -EBADFD;

	/*
	 * When USB handset is used, we have a second soundcard, so we need
	 * to change the control we would use. This solution is kind of a of
	 * hack but i have no better idea how to solve this issue right now.
	 */
	if (audio->usb_handset && audio_is_handset_selected(audio) &&
	    g_list_length(audio->cards) > 1) {
		struct soundcard *usb;

		usb = audio_get_card_by_name(audio, AUDIO_USB_DEVICE_NAME);
		if (!usb) {
			g_warning("audio-alsa-ucm: usb handset not found");
			return -ENODEV;
		}

		card = usb;
		control = "PCM";
		g_debug("   overriding handset: card=%s control=%s",
			card->name, control);
	}

	return soundcard_control_set_volume(card, control, volume);
}

int audio_get_volume(struct audio *audio, uint8_t *volumep)
{
	struct soundcard *card;
	const char *control;
	long volume = 0;
	int ret;

	if (!volumep)
		return -EINVAL;

	control = audio_state_to_control(audio);
	if (!control)
		return -EBADFD;

	card = g_list_nth(audio->cards, audio->index)->data;
	if (!card)
		return -EBADFD;

	/*
	 * When USB handset is used, we have a second soundcard, so we need
	 * to change the control we would use. This solution is kind of a of
	 * hack but i have no better idea how to solve this issue right now.
	 */
	if (audio->usb_handset && audio_is_handset_selected(audio) &&
	    g_list_length(audio->cards) > 1) {
		struct soundcard *usb;

		usb = audio_get_card_by_name(audio, AUDIO_USB_DEVICE_NAME);
		if (!usb) {
			g_warning("audio-alsa-ucm: usb handset not found");
			return -ENODEV;
		}

		card = usb;
		control = "PCM";
		g_debug("   overriding handset: card=%s control=%s",
			card->name, control);
	}

	ret = soundcard_control_get_volume(card, control, &volume);
	if (ret < 0)
		return ret;

	*volumep = volume;
	return 0;
}

int audio_set_speakers_enable(struct audio *audio, bool enable)
{
	return -ENOSYS;
}

int audio_get_speakers_enable(struct audio *audio, bool *enablep)
{
	return -ENOSYS;
}
