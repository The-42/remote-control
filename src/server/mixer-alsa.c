/*
 * Copyright (C) 2010-2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <pthread.h>
#include <sys/poll.h>
#include <alsa/asoundlib.h>

#include "remote-control-stub.h"
#include "remote-control.h"

struct mixer_element;

struct mixer_element_ops {
	int (*get_volume_range)(snd_mixer_elem_t *element, long *min, long *max);
	int (*get_volume)(snd_mixer_elem_t *element, long *value);
	int (*set_volume)(snd_mixer_elem_t *element, long value);
	int (*get_switch)(snd_mixer_elem_t *element, int *value);
	int (*set_switch)(snd_mixer_elem_t *element, int value);
};

struct mixer_element {
	struct mixer_element_ops *ops;
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

static int normalize_volume(struct mixer_element *element, long *volume)
{
	long min = 0;
	long max = 0;
	int err;

	err = element->ops->get_volume_range(element->element, &min, &max);
	if (err < 0)
		return err;

	*volume = ((*volume - min) * 255) / (max - min);
	return 0;
}

static int scale_volume(struct mixer_element *element, long *volume)
{
	long min = 0;
	long max = 0;
	int err;

	err = element->ops->get_volume_range(element->element, &min, &max);
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

static struct mixer_element_ops playback_ops = {
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

static struct mixer_element_ops capture_ops = {
	.get_volume_range = snd_mixer_selem_get_capture_volume_range,
	.get_volume = snd_mixer_selem_get_capture_volume_all,
	.set_volume = snd_mixer_selem_set_capture_volume_all,
	.get_switch = snd_mixer_selem_get_capture_switch_all,
	.set_switch = snd_mixer_selem_set_capture_switch_all,
};

static int mixer_element_create(struct mixer_element **elementp, snd_mixer_elem_t *elem)
{
	struct mixer_element *element;
	const char *name;

	if (!elementp || !elem)
		return -EINVAL;

	element = calloc(1, sizeof(*element));
	if (!element)
		return -ENOMEM;

	name = snd_mixer_selem_get_name(elem);

	element->element = elem;

	if (snd_mixer_selem_has_playback_volume(elem)) {
		rc_log(RC_DEBUG "element \"%s\" is playback control\n", name);
		element->ops = &playback_ops;
	}

	if (snd_mixer_selem_has_capture_volume(elem)) {
		rc_log(RC_DEBUG "element \"%s\" is capture control\n", name);
		element->ops = &capture_ops;
	}

	*elementp = element;
	return 0;
}

static int mixer_element_free(struct mixer_element *element)
{
	if (!element)
		return -EINVAL;

	free(element);
	return 0;
}

struct mixer {
	snd_mixer_t *mixer;
	pthread_t thread;
	int timeout;
	bool done;

	struct mixer_element *elements[MIXER_CONTROL_MAX];
	int input_source[MIXER_INPUT_SOURCE_MAX];
	unsigned int input_source_bits;
	snd_mixer_elem_t *input;
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
	int i;

	if (!mixerp)
		return -EINVAL;

	mixer = calloc(1, sizeof(*mixer));
	if (!mixer)
		return -ENOMEM;

	for (i = 0; i < MIXER_INPUT_SOURCE_MAX; i++)
		mixer->input_source[i] = -1;

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
		struct mixer_element *element = NULL;
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

			err = mixer_element_create(&element, elem);
			if (err < 0) {
				rc_log(RC_ERR "mixer_control_create(): %s\n",
						strerror(-err));
				continue;
			}

			rc_log(RC_DEBUG "\"%s\" is %s playback control\n",
					name, desc);
			mixer->elements[type] = element;
			continue;
		}

		if (snd_mixer_selem_has_capture_volume(elem)) {
			if ((strcmp(name, "Capture") == 0) && (index == 0)) {
				type = MIXER_CONTROL_CAPTURE_MASTER;
				desc = "master";
			}

			if (!desc)
				continue;

			err = mixer_element_create(&element, elem);
			if (err < 0) {
				rc_log(RC_ERR "mixer_control_create(): %s\n",
						strerror(-err));
				continue;
			}

			rc_log(RC_DEBUG "\"%s\" is master capture control\n",
					name);
			mixer->elements[type] = element;
			continue;
		}

		if (snd_mixer_selem_is_enum_capture(elem)) {
			int num = snd_mixer_selem_get_enum_items(elem);
			char buf[16];

			if (mixer->input)
				continue;

			if ((strcmp(name, "Input Source") == 0) && (index == 0))
				mixer->input = elem;

			for (i = 0; i < num; i++) {
				err = snd_mixer_selem_get_enum_item_name(elem, i, sizeof(buf), buf);
				if (err < 0)
					continue;

				if (strcmp(buf, "Mic") == 0) {
					mixer->input_source[MIXER_INPUT_SOURCE_HEADSET] = i;
					rc_log(RC_DEBUG "%s is headset source\n", buf);
					continue;
				}

				if (strcmp(buf, "Internal Mic") == 0) {
					mixer->input_source[MIXER_INPUT_SOURCE_HANDSET] = i;
					rc_log(RC_DEBUG "%s is handset source\n", buf);
					continue;
				}

				if (strcmp(buf, "Line") == 0) {
					mixer->input_source[MIXER_INPUT_SOURCE_LINE] = i;
					rc_log(RC_DEBUG "%s is line source\n", buf);
					continue;
				}
			}

			mixer->input_source_bits = 0;

			for (i = 0; i <= SND_MIXER_SCHN_LAST; i++) {
				unsigned int item = 0;

				if (snd_mixer_selem_get_enum_item(elem, i, &item) >= 0)
					mixer->input_source_bits |= 1 << i;
			}
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
		mixer_element_free(mixer->elements[i]);

	snd_mixer_close(mixer->mixer);
	free(mixer);
	return 0;
}

int mixer_set_volume(struct mixer *mixer, unsigned short control, unsigned int volume)
{
	struct mixer_element *element;
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

	element = mixer->elements[control];
	if (!element || !element->element) {
		ret = -ENODEV;
		goto out;
	}

	name = snd_mixer_selem_get_name(element->element);

	if (!element->ops || !element->ops->set_volume) {
		ret = -ENOSYS;
		goto out;
	}

	err = scale_volume(element, &value);
	if (err < 0) {
		rc_log(RC_DEBUG "failed to scale volume for control %s: %s\n",
				name, snd_strerror(err));
		ret = err;
		goto out;
	}

	err = element->ops->set_volume(element->element, value);
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
	struct mixer_element *element;
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

	element = mixer->elements[control];
	if (!element || !element->element) {
		ret = -ENODEV;
		goto out;
	}

	name = snd_mixer_selem_get_name(element->element);

	if (!element->ops || !element->ops->get_volume) {
		ret = -ENOSYS;
		goto out;
	}

	err = element->ops->get_volume(element->element, &value);
	if (err < 0) {
		rc_log(RC_DEBUG "failed to get volume for control %s: %s\n",
				name, snd_strerror(err));
		ret = err;
		goto out;
	}

	err = normalize_volume(element, &value);
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
	struct mixer_element *element;
	const char *name;
	int ret = 0;
	int err;

	rc_log(RC_DEBUG "> %s(mixer=%p, control=%u, mute=%s)\n", __func__,
			mixer, control, mute ? "true" : "false");

	if (control >= MIXER_CONTROL_MAX)
		return -EINVAL;

	element = mixer->elements[control];
	if (!element || !element->element) {
		ret = -ENODEV;
		goto out;
	}

	name = snd_mixer_selem_get_name(element->element);

	if (!element->ops || !element->ops->set_switch) {
		ret = -ENOSYS;
		goto out;
	}

	err = element->ops->set_switch(element->element, !mute);
	if (err < 0)
		ret = err;

out:
	rc_log(RC_DEBUG "< %s() = %d\n", __func__, ret);
	return ret;
}

int mixer_is_muted(struct mixer *mixer, unsigned short control, bool *mutep)
{
	struct mixer_element *element;
	int value = 0;
	int ret = 0;
	int err;

	rc_log(RC_DEBUG "> %s(mixer=%p, control=%u, mutep=%p)\n", __func__,
			mixer, control, mutep);

	if (control >= MIXER_CONTROL_MAX) {
		ret = -EINVAL;
		goto out;
	}

	element = mixer->elements[control];
	if (!element || !element->element) {
		ret = -ENODEV;
		goto out;
	}

	if (!element->ops || !element->ops->get_switch) {
		ret = -ENOSYS;
		goto out;
	}

	err = element->ops->get_switch(element->element, &value);
	if (!err)
		*mutep = !value;
	else
		ret = err;

out:
	rc_log(RC_DEBUG "< %s() = %d\n", __func__, ret);
	return ret;
}

int mixer_set_input_source(struct mixer *mixer, enum mixer_input_source source)
{
	unsigned int i;

	if (source >= MIXER_INPUT_SOURCE_MAX)
		return -EINVAL;

	for (i = 0; i <= SND_MIXER_SCHN_LAST; i++) {
		if (mixer->input_source_bits & (1 << i)) {
			int err = snd_mixer_selem_set_enum_item(mixer->input,
					i, mixer->input_source[source]);
			if (err < 0)
				return err;
		}
	}

	return 0;
}

int mixer_get_input_source(struct mixer *mixer, enum mixer_input_source *sourcep)
{
	unsigned int index = 0;
	unsigned short i;
	int err;

	if (!sourcep)
		return -EINVAL;

	err = snd_mixer_selem_get_enum_item(mixer->input, 0, &index);
	if (err < 0)
		return err;

	for (i = 0; i < MIXER_INPUT_SOURCE_MAX; i++) {
		if (mixer->input_source[i] == index) {
			*sourcep = i;
			break;
		}
	}

	return 0;
}
