#include <alsa/asoundlib.h>

#include "remote-control-stub.h"
#include "remote-control.h"

struct mixer_control;

struct mixer_control_ops {
	int (*get_volume)(struct mixer_control *ctl, unsigned int *volumep);
	int (*set_volume)(struct mixer_control *ctl, unsigned int volume);
	int (*get_mute)(struct mixer_control *ctl, bool *mutep);
	int (*set_mute)(struct mixer_control *ctl, bool mute);
};

struct mixer_control {
	struct mixer_control_ops *ops;
	snd_mixer_elem_t *element;
};

static int playback_get_volume(struct mixer_control *ctl, unsigned int *volumep)
{
	snd_mixer_selem_channel_id_t channel = SND_MIXER_SCHN_UNKNOWN;
	const char *name;
	long value = 0;
	int ret = 0;
	int err;

	rc_log(RC_DEBUG "> %s(ctl=%p, volumep=%p)\n", __func__, ctl, volumep);

	if (!ctl->element) {
		ret = -EINVAL;
		goto out;
	}

	if (!snd_mixer_selem_has_playback_volume(ctl->element)) {
		ret = -EINVAL;
		goto out;
	}

	name = snd_mixer_selem_get_name(ctl->element);

	if (!snd_mixer_selem_has_playback_volume_joined(ctl->element))
		rc_log(RC_DEBUG "TODO: select channel\n");
	else
		channel = SND_MIXER_SCHN_MONO;

	err = snd_mixer_selem_get_playback_volume(ctl->element, channel, &value);
	if (err != 0) {
		rc_log(RC_DEBUG "failed to get playback volume for control %s: %s\n",
				name, snd_strerror(err));
		ret = -EIO;
	} else {
		/* TODO: normalize */
		*volumep = value;
	}

out:
	rc_log(RC_DEBUG "< %s() = %d\n", __func__, ret);
	return ret;
}

static int playback_set_volume(struct mixer_control *ctl, unsigned int volume)
{
	snd_mixer_selem_channel_id_t channel = SND_MIXER_SCHN_UNKNOWN;
	const char *name;
	int ret = 0;
	long value;
	int err;

	rc_log(RC_DEBUG "> %s(ctl=%p, volume=%u)\n", __func__, ctl, volume);

	if (!ctl->element) {
		ret = -EINVAL;
		goto out;
	}

	if (!snd_mixer_selem_has_playback_volume(ctl->element)) {
		ret = -EINVAL;
		goto out;
	}

	name = snd_mixer_selem_get_name(ctl->element);

	if (!snd_mixer_selem_has_playback_volume_joined(ctl->element))
		rc_log(RC_DEBUG "TODO: select channel\n");
	else
		channel = SND_MIXER_SCHN_MONO;

	/* TODO: denormalize */
	value = volume;

	err = snd_mixer_selem_set_playback_volume_all(ctl->element, value);
	if (err != 0) {
		rc_log(RC_DEBUG "failed to set playback volume for control "
				"%s: %s\n", name, snd_strerror(err));
		ret = -EIO;
	}

out:
	rc_log(RC_DEBUG "< %s() = %d\n", __func__, ret);
	return ret;
}

static int playback_get_mute(struct mixer_control *ctl, bool *mutep)
{
	snd_mixer_selem_channel_id_t channel = SND_MIXER_SCHN_UNKNOWN;
	int value = 0;
	int ret = 0;
	int err;

	rc_log(RC_DEBUG "> %s(ctl=%p, mutep=%p)\n", __func__, ctl, mutep);

	if (!ctl->element) {
		ret = -EINVAL;
		goto out;
	}

	if (!snd_mixer_selem_has_playback_switch(ctl->element)) {
		ret = -EINVAL;
		goto out;
	}

	if (!snd_mixer_selem_has_playback_switch_joined(ctl->element))
		rc_log(RC_DEBUG "  TODO: select channel\n");
	else
		channel = SND_MIXER_SCHN_MONO;

	err = snd_mixer_selem_get_playback_switch(ctl->element, channel, &value);
	if (err != 0) {
		rc_log(RC_DEBUG "  snd_mixer_selem_get_playback_switch(): %s\n", snd_strerror(err));
		ret = -EIO;
	} else {
		*mutep = !value;
	}

out:
	rc_log(RC_DEBUG "< %s() = %d\n", __func__, ret);
	return ret;
}

static int playback_set_mute(struct mixer_control *ctl, bool mute)
{
	snd_mixer_selem_channel_id_t channel = SND_MIXER_SCHN_UNKNOWN;
	const char *name;
	int ret = 0;
	int err;

	rc_log(RC_DEBUG "> %s(ctl=%p, mute=%u)\n", __func__, ctl, mute);

	if (!ctl->element) {
		ret = -EINVAL;
		goto out;
	}

	name = snd_mixer_selem_get_name(ctl->element);

	if (!snd_mixer_selem_has_playback_switch(ctl->element)) {
		ret = -EINVAL;
		goto out;
	}

	if (!snd_mixer_selem_has_playback_switch_joined(ctl->element))
		rc_log(RC_DEBUG "  TODO: select channel\n");
	else
		channel = SND_MIXER_SCHN_MONO;

	err = snd_mixer_selem_set_playback_switch(ctl->element, channel, !mute);
	if (err != 0) {
		rc_log(RC_DEBUG "failed to %smute control %s: %s\n",
				mute ? "" : "un", name, snd_strerror(err));
		ret = -EIO;
	}

out:
	rc_log(RC_DEBUG "< %s() = %d\n", __func__, ret);
	return ret;
}

static struct mixer_control_ops playback_ops = {
	.get_volume = playback_get_volume,
	.set_volume = playback_set_volume,
	.get_mute = playback_get_mute,
	.set_mute = playback_set_mute,
};

static int capture_get_volume(struct mixer_control *ctl, unsigned int *volumep)
{
	int ret = -ENOSYS;
	rc_log(RC_DEBUG "> %s(ctl=%p, volumep=%p)\n", __func__, ctl, volumep);
	rc_log(RC_DEBUG "< %s() = %d\n", __func__, ret);
	return ret;
}

static int capture_set_volume(struct mixer_control *ctl, unsigned int volume)
{
	int ret = -ENOSYS;
	rc_log(RC_DEBUG "> %s(ctl=%p, volume=%u)\n", __func__, ctl, volume);
	rc_log(RC_DEBUG "< %s() = %d\n", __func__, ret);
	return ret;
}

static int capture_get_mute(struct mixer_control *ctl, bool *mutep)
{
	int ret = -ENOSYS;
	rc_log(RC_DEBUG "> %s(ctl=%p, mutep=%p)\n", __func__, ctl, mutep);
	rc_log(RC_DEBUG "< %s() = %d\n", __func__, ret);
	return ret;
}

static int capture_set_mute(struct mixer_control *ctl, bool mute)
{
	int ret = -ENOSYS;
	rc_log(RC_DEBUG "> %s(ctl=%p, mute=%u)\n", __func__, ctl, mute);
	rc_log(RC_DEBUG "< %s() = %d\n", __func__, ret);
	return ret;
}

static struct mixer_control_ops capture_ops = {
	.get_volume = capture_get_volume,
	.set_volume = capture_set_volume,
	.get_mute = capture_get_mute,
	.set_mute = capture_set_mute,
};

static int mixer_control_create(struct mixer_control **controlp, snd_mixer_elem_t *element)
{
	struct mixer_control *control;
	const char *name;

	if (!controlp || !element)
		return -EINVAL;

	control = calloc(1, sizeof(*control));
	if (!control)
		return -ENOMEM;

	name = snd_mixer_selem_get_name(element);

	control->element = element;

	if (snd_mixer_selem_has_playback_volume(element)) {
		rc_log(RC_DEBUG "element \"%s\" is playback control\n", name);
		control->ops = &playback_ops;
	}

	if (snd_mixer_selem_has_capture_volume(element)) {
		rc_log(RC_DEBUG "element \"%s\" is capture control\n", name);
		control->ops = &capture_ops;
	}

	*controlp = control;
	return 0;
}

static int mixer_control_free(struct mixer_control *control)
{
	if (!control)
		return -EINVAL;

	free(control);
	return 0;
}

struct mixer {
	snd_mixer_t *mixer;

	struct mixer_control *controls[MIXER_CONTROL_MAX];
};

int mixer_create(struct mixer **mixerp)
{
	static const char card[] = "default";
	struct mixer *mixer = NULL;
	snd_mixer_selem_id_t *sid;
	snd_mixer_elem_t *elem;
	int err;

	if (!mixerp)
		return -EINVAL;

	mixer = calloc(1, sizeof(*mixer));
	if (!mixer)
		return -ENOMEM;

	err = snd_mixer_open(&mixer->mixer, 0);
	if (err < 0) {
		rc_log(RC_NOTICE "snd_mixer_open(): %s\n", snd_strerror(err));
		free(mixer);
		return -ENODEV;
	}

	err = snd_mixer_attach(mixer->mixer, card);
	if (err < 0) {
		rc_log(RC_NOTICE "snd_mixer_attach(): %s\n", snd_strerror(err));
		snd_mixer_close(mixer->mixer);
		free(mixer);
		return -ENODEV;
	}

	err = snd_mixer_selem_register(mixer->mixer, NULL, NULL);
	if (err < 0) {
		rc_log(RC_NOTICE "snd_mixer_selem_register(): %s\n",
				snd_strerror(err));
		snd_mixer_close(mixer->mixer);
		free(mixer);
		return -ENODEV;
	}

	err = snd_mixer_load(mixer->mixer);
	if (err < 0) {
		rc_log(RC_NOTICE "snd_mixer_load(): %s\n", snd_strerror(err));
		snd_mixer_close(mixer->mixer);
		free(mixer);
		return -ENODEV;
	}

	snd_mixer_selem_id_alloca(&sid);

	for (elem = snd_mixer_first_elem(mixer->mixer); elem; elem = snd_mixer_elem_next(elem)) {
		enum medcom_mixer_control type = MIXER_CONTROL_UNKNOWN;
		const char *name = snd_mixer_selem_get_name(elem);
		int index = snd_mixer_selem_get_index(elem);
		struct mixer_control *control = NULL;
		const char *desc = NULL;

		snd_mixer_selem_get_id(elem, sid);

		if (snd_mixer_selem_has_playback_volume(elem)) {
			if (strcmp(name, "Master") == 0) {
				type = MIXER_CONTROL_PLAYBACK_MASTER;
				desc = "master";
			}

			if (strcmp(name, "PCM") == 0) {
				type = MIXER_CONTROL_PLAYBACK_PCM;
				desc = "PCM";
			}

			if (strcmp(name, "Headphone") == 0) {
				type = MIXER_CONTROL_PLAYBACK_HEADSET;
				desc = "headset";
			}

			if (strcmp(name, "Speaker") == 0) {
				type = MIXER_CONTROL_PLAYBACK_SPEAKER;
				desc = "speaker";
			}

			if (strcmp(name, "Front") == 0) {
				type = MIXER_CONTROL_PLAYBACK_HANDSET;
				desc = "handset";
			}

			if (!desc)
				continue;

			err = mixer_control_create(&control, elem);
			if (err < 0) {
				rc_log(RC_ERR "mixer_control_create(): %s\n",
						strerror(-err));
				continue;
			}

			rc_log(RC_DEBUG "\"%s\" is %s playback control\n",
					name, desc);
			mixer->controls[type] = control;
			continue;
		}

		if (snd_mixer_selem_has_capture_volume(elem)) {
			if ((strcmp(name, "Capture") == 0) && (index == 0)) {
				type = MIXER_CONTROL_CAPTURE_MASTER;
				desc = "master";
			}

			if (!desc)
				continue;

			err = mixer_control_create(&control, elem);
			if (err < 0) {
				rc_log(RC_ERR "mixer_control_create(): %s\n",
						strerror(-err));
				continue;
			}

			rc_log(RC_DEBUG "\"%s\" is master capture control\n",
					name);
			mixer->controls[type] = control;
			continue;
		}
	}

	*mixerp = mixer;
	return 0;
}

int mixer_free(struct mixer *mixer)
{
	unsigned int i;

	if (!mixer)
		return -EINVAL;

	for (i = 0; i < MIXER_CONTROL_MAX; i++)
		mixer_control_free(mixer->controls[i]);

	snd_mixer_close(mixer->mixer);
	free(mixer);
	return 0;
}

int mixer_set_volume(struct mixer *mixer, unsigned short control, unsigned int volume)
{
	struct mixer_control *ctl;

	if (control >= MIXER_CONTROL_MAX)
		return -EINVAL;

	ctl = mixer->controls[control];
	if (!ctl || !ctl->ops)
		return -ENODEV;

	if (!ctl->ops || !ctl->ops->set_volume)
		return -ENOSYS;

	return ctl->ops->set_volume(ctl, volume);
}

int mixer_get_volume(struct mixer *mixer, unsigned short control, unsigned int *volumep)
{
	struct mixer_control *ctl;

	if (control >= MIXER_CONTROL_MAX)
		return -EINVAL;

	ctl = mixer->controls[control];
	if (!ctl || !ctl->ops)
		return -ENODEV;

	if (!ctl->ops || !ctl->ops->get_volume)
		return -ENOSYS;

	return ctl->ops->get_volume(ctl, volumep);
}

int mixer_set_mute(struct mixer *mixer, unsigned short control, bool mute)
{
	struct mixer_control *ctl;

	if (control >= MIXER_CONTROL_MAX)
		return -EINVAL;

	ctl = mixer->controls[control];
	if (!ctl || !ctl->ops)
		return -ENODEV;

	if (!ctl->ops || !ctl->ops->set_mute)
		return -ENOSYS;

	return ctl->ops->set_mute(ctl, mute);
}

int mixer_is_muted(struct mixer *mixer, unsigned short control, bool *mutep)
{
	struct mixer_control *ctl;

	rc_log(RC_DEBUG "> %s(mixer=%p, control=%u, mutep=%p)\n", __func__,
			mixer, control, mutep);

	if (control >= MIXER_CONTROL_MAX)
		return -EINVAL;

	ctl = mixer->controls[control];
	if (!ctl || !ctl->ops)
		return -ENODEV;

	if (!ctl->ops || !ctl->ops->get_mute)
		return -ENOSYS;

	return ctl->ops->get_mute(ctl, mutep);
}
