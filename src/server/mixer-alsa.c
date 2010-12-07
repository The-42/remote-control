#include <pthread.h>
#include <sys/poll.h>
#include <alsa/asoundlib.h>

#include "remote-control-stub.h"
#include "remote-control.h"

struct mixer_control;

struct mixer_control_ops {
	int (*get_volume_range)(snd_mixer_elem_t *element, long *min, long *max);
	int (*get_volume)(snd_mixer_elem_t *element, long *value);
	int (*set_volume)(snd_mixer_elem_t *element, long value);
	int (*get_switch)(snd_mixer_elem_t *element, int *value);
	int (*set_switch)(snd_mixer_elem_t *element, int value);
};

struct mixer_control {
	struct mixer_control_ops *ops;
	snd_mixer_elem_t *element;
};

static int element_callback(snd_mixer_elem_t *element, unsigned int mask)
{
	return 0;
}

static int mixer_callback(snd_mixer_t *mixer, unsigned int mask,
		snd_mixer_elem_t *element)
{
	if (mask & SND_CTL_EVENT_MASK_ADD)
		snd_mixer_elem_set_callback(element, element_callback);

	return 0;
}

static int normalize_volume(struct mixer_control *control, long *volume)
{
	long min = 0;
	long max = 0;
	int err;

	err = control->ops->get_volume_range(control->element, &min, &max);
	if (err < 0)
		return err;

	*volume = ((*volume - min) * 255) / (max - min);
	return 0;
}

static int scale_volume(struct mixer_control *control, long *volume)
{
	long min = 0;
	long max = 0;
	int err;

	err = control->ops->get_volume_range(control->element, &min, &max);
	if (err < 0)
		return err;

	*volume = ((*volume * (max - min)) / 255) + min;
	return 0;
}

static int snd_mixer_selem_get_playback_volume_all(snd_mixer_elem_t *element,
		long *value)
{
	return snd_mixer_selem_get_playback_volume(element,
			SND_MIXER_SCHN_MONO, value);
}

static int snd_mixer_selem_get_playback_switch_all(snd_mixer_elem_t *element,
		int *value)
{
	return snd_mixer_selem_get_playback_switch(element,
			SND_MIXER_SCHN_MONO, value);
}

static struct mixer_control_ops playback_ops = {
	.get_volume_range = snd_mixer_selem_get_playback_volume_range,
	.get_volume = snd_mixer_selem_get_playback_volume_all,
	.set_volume = snd_mixer_selem_set_playback_volume_all,
	.get_switch = snd_mixer_selem_get_playback_switch_all,
	.set_switch = snd_mixer_selem_set_playback_switch_all,
};

static int snd_mixer_selem_get_capture_volume_all(snd_mixer_elem_t *element,
		long *value)
{
	return snd_mixer_selem_get_capture_volume(element,
			SND_MIXER_SCHN_MONO, value);
}

static int snd_mixer_selem_get_capture_switch_all(snd_mixer_elem_t *element,
		int *value)
{
	return snd_mixer_selem_get_capture_switch(element,
			SND_MIXER_SCHN_MONO, value);
}

static struct mixer_control_ops capture_ops = {
	.get_volume_range = snd_mixer_selem_get_capture_volume_range,
	.get_volume = snd_mixer_selem_get_capture_volume_all,
	.set_volume = snd_mixer_selem_set_capture_volume_all,
	.get_switch = snd_mixer_selem_get_capture_switch_all,
	.set_switch = snd_mixer_selem_set_capture_switch_all,
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
	pthread_t thread;
	int timeout;
	bool done;

	struct mixer_control *controls[MIXER_CONTROL_MAX];
};

static void *poll_thread(void *data)
{
	struct mixer *mixer = data;
	struct pollfd *fds = NULL;
	int num = 0;

	while (!mixer->done) {
		unsigned short events = 0;
		int err;

		err = snd_mixer_poll_descriptors_count(mixer->mixer);
		if (err < 0) {
			rc_log(RC_ERR "snd_mixer_poll_descriptors_count(): %s\n",
					snd_strerror(err));
			break;
		}

		if (err != num) {
			if (fds)
				free(fds);

			fds = calloc(num, sizeof(*fds));
			if (!fds) {
				rc_log(RC_ERR "failed to allocate file poll "
						"descriptors\n");
				break;
			}

			num = err;
		}

		err = snd_mixer_poll_descriptors(mixer->mixer, fds, num);
		if (err < 0) {
			rc_log(RC_ERR "snd_mixer_poll_descriptors(): %s\n",
					snd_strerror(err));
			break;
		}

		err = poll(fds, num, mixer->timeout);
		if (err < 0) {
			rc_log(RC_ERR "poll(): %s\n", snd_strerror(err));
			break;
		}

		err = snd_mixer_poll_descriptors_revents(mixer->mixer, fds, num, &events);
		if (err < 0) {
			rc_log(RC_ERR "snd_mixer_poll_descriptors_revents(): %s\n",
					snd_strerror(err));
			break;
		}

		if (events & POLLIN)
			snd_mixer_handle_events(mixer->mixer);
	}

	if (fds)
		free(fds);

	return NULL;
}

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

	snd_mixer_set_callback(mixer->mixer, mixer_callback);

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

	err = pthread_create(&mixer->thread, NULL, poll_thread, mixer);
	if (err < 0) {
		rc_log(RC_ERR "failed to create polling thread: %s\n",
				strerror(-err));
		return err;
	}

	mixer->timeout = 250;

	*mixerp = mixer;
	return 0;
}

int mixer_free(struct mixer *mixer)
{
	unsigned int i;

	if (!mixer)
		return -EINVAL;

	mixer->done = true;
	pthread_join(mixer->thread, NULL);

	for (i = 0; i < MIXER_CONTROL_MAX; i++)
		mixer_control_free(mixer->controls[i]);

	snd_mixer_close(mixer->mixer);
	free(mixer);
	return 0;
}

int mixer_set_volume(struct mixer *mixer, unsigned short control, unsigned int volume)
{
	struct mixer_control *ctl;
	long value = volume;
	const char *name;
	int ret = 0;
	int err;

	rc_log(RC_DEBUG "> %s(mixer=%p, control=%u, volume=%u)\n", __func__,
			mixer, control, volume);

	if (control >= MIXER_CONTROL_MAX) {
		ret = -EINVAL;
		goto out;
	}

	ctl = mixer->controls[control];
	if (!ctl || !ctl->element) {
		ret = -ENODEV;
		goto out;
	}

	name = snd_mixer_selem_get_name(ctl->element);

	if (!ctl->ops || !ctl->ops->set_volume) {
		ret = -ENOSYS;
		goto out;
	}

	err = scale_volume(ctl, &value);
	if (err < 0) {
		rc_log(RC_DEBUG "failed to scale volume for control %s: %s\n",
				name, snd_strerror(err));
		ret = err;
		goto out;
	}

	err = ctl->ops->set_volume(ctl->element, value);
	if (err < 0) {
		rc_log(RC_DEBUG "failed to set volume for control %s: %s\n",
				name, snd_strerror(err));
		ret = err;
	}

out:
	rc_log(RC_DEBUG "< %s() = %d\n", __func__, ret);
	return ret;
}

int mixer_get_volume(struct mixer *mixer, unsigned short control, unsigned int *volumep)
{
	struct mixer_control *ctl;
	const char *name;
	long value = 0;
	int ret = 0;
	int err;

	rc_log(RC_DEBUG "> %s(mixer=%p, control=%u, volumep=%p)\n", __func__,
			mixer, control, volumep);

	if (control >= MIXER_CONTROL_MAX) {
		ret = -EINVAL;
		goto out;
	}

	ctl = mixer->controls[control];
	if (!ctl || !ctl->element) {
		ret = -ENODEV;
		goto out;
	}

	name = snd_mixer_selem_get_name(ctl->element);

	if (!ctl->ops || !ctl->ops->get_volume) {
		ret = -ENOSYS;
		goto out;
	}

	err = ctl->ops->get_volume(ctl->element, &value);
	if (err < 0) {
		rc_log(RC_DEBUG "failed to get volume for control %s: %s\n",
				name, snd_strerror(err));
		ret = err;
		goto out;
	}

	err = normalize_volume(ctl, &value);
	if (err < 0) {
		rc_log(RC_DEBUG "failed to normalize volume for control %s: "
				"%s\n", name, snd_strerror(err));
		goto out;
	}

	*volumep = value;

out:
	rc_log(RC_DEBUG "< %s() = %d\n", __func__, ret);
	return ret;
}

int mixer_set_mute(struct mixer *mixer, unsigned short control, bool mute)
{
	struct mixer_control *ctl;
	const char *name;
	int ret = 0;
	int err;

	rc_log(RC_DEBUG "> %s(mixer=%p, control=%u, mute=%s)\n", __func__,
			mixer, control, mute ? "true" : "false");

	if (control >= MIXER_CONTROL_MAX)
		return -EINVAL;

	ctl = mixer->controls[control];
	if (!ctl || !ctl->element) {
		ret = -ENODEV;
		goto out;
	}

	name = snd_mixer_selem_get_name(ctl->element);

	if (!ctl->ops || !ctl->ops->set_switch) {
		ret = -ENOSYS;
		goto out;
	}

	err = ctl->ops->set_switch(ctl->element, !mute);
	if (err < 0)
		ret = err;

out:
	rc_log(RC_DEBUG "< %s() = %d\n", __func__, ret);
	return ret;
}

int mixer_is_muted(struct mixer *mixer, unsigned short control, bool *mutep)
{
	struct mixer_control *ctl;
	int value = 0;
	int ret = 0;
	int err;

	rc_log(RC_DEBUG "> %s(mixer=%p, control=%u, mutep=%p)\n", __func__,
			mixer, control, mutep);

	if (control >= MIXER_CONTROL_MAX) {
		ret = -EINVAL;
		goto out;
	}

	ctl = mixer->controls[control];
	if (!ctl || !ctl->element) {
		ret = -ENODEV;
		goto out;
	}

	if (!ctl->ops || !ctl->ops->get_switch) {
		ret = -ENOSYS;
		goto out;
	}

	err = ctl->ops->get_switch(ctl->element, &value);
	if (!err)
		*mutep = !value;
	else
		ret = err;

out:
	rc_log(RC_DEBUG "< %s() = %d\n", __func__, ret);
	return ret;
}
