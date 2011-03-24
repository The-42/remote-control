/*
 * Copyright (C) 2010-2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <sys/poll.h>
#include <alsa/asoundlib.h>
#include <glib.h>

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

	element = g_new0(struct mixer_element, 1);
	if (!element)
		return -ENOMEM;

	name = snd_mixer_selem_get_name(elem);

	element->element = elem;

	if (snd_mixer_selem_has_playback_volume(elem)) {
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "element \"%s\" is "
				"playback control", name);
		element->ops = &playback_ops;
	}

	if (snd_mixer_selem_has_capture_volume(elem)) {
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "element \"%s\" is "
				"capture control", name);
		element->ops = &capture_ops;
	}

	*elementp = element;
	return 0;
}

static int mixer_element_free(struct mixer_element *element)
{
	if (!element)
		return -EINVAL;

	g_free(element);
	return 0;
}

struct mixer {
	GSource source;
	unsigned int num_fds;
	GPollFD *fds;

	snd_mixer_t *mixer;
	int timeout;

	struct mixer_element *elements[MIXER_CONTROL_MAX];
	int input_source[MIXER_INPUT_SOURCE_MAX];
	unsigned int input_source_bits;
	snd_mixer_elem_t *input;

	GThread *thread;
	gboolean loop;
};

static gboolean mixer_source_prepare(GSource *source, gint *timeout)
{
	if (timeout)
		*timeout = -1;

	return FALSE;
}

static gboolean mixer_source_check(GSource *source)
{
	struct mixer *mixer = (struct mixer *)source;
	unsigned int i;

	for (i = 0; i < mixer->num_fds; i++) {
		if (mixer->fds[i].revents & G_IO_IN)
			return TRUE;
	}

	return FALSE;
}

static gboolean mixer_source_dispatch(GSource *source, GSourceFunc callback,
		gpointer user_data)
{
	struct mixer *mixer = (struct mixer *)source;

	snd_mixer_handle_events(mixer->mixer);

	if (callback)
		return callback(user_data);

	return TRUE;
}

static void mixer_source_finalize(GSource *source)
{
	struct mixer *mixer = (struct mixer *)source;
	unsigned int i;

	if ((mixer->thread != NULL) || mixer->loop) {
		mixer->loop = FALSE;
		g_thread_join(mixer->thread);
	}

	for (i = 0; i < MIXER_CONTROL_MAX; i++)
		mixer_element_free(mixer->elements[i]);

	snd_mixer_close(mixer->mixer);
}

static GSourceFuncs mixer_source_funcs = {
	.prepare = mixer_source_prepare,
	.check = mixer_source_check,
	.dispatch = mixer_source_dispatch,
	.finalize = mixer_source_finalize,
};

static int mixer_probe(struct mixer *mixer)
{
	snd_mixer_selem_id_t *sid;
	snd_mixer_elem_t *elem;
	int err;
	int i;

	snd_mixer_selem_id_alloca(&sid);

	for (elem = snd_mixer_first_elem(mixer->mixer); elem; elem = snd_mixer_elem_next(elem)) {
		const char *name = snd_mixer_selem_get_name(elem);
		enum mixer_control type = MIXER_CONTROL_UNKNOWN;
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
				g_log(G_LOG_DOMAIN, G_LOG_LEVEL_ERROR,
						"failed to create element: "
						"%s", strerror(-err));
				continue;
			}

			g_log(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "\"%s\" is %s "
					"playback control", name, desc);
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
				g_log(G_LOG_DOMAIN, G_LOG_LEVEL_ERROR,
						"failed to create element: "
						"%s", strerror(-err));
				continue;
			}

			g_log(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "\"%s\" is "
					"master capture control", name);
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
					g_log(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
							"\"%s\" is headset "
							"source", buf);
					continue;
				}

				if (strcmp(buf, "Internal Mic") == 0) {
					mixer->input_source[MIXER_INPUT_SOURCE_HANDSET] = i;
					g_log(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
							"\"%s\" is handset "
							"source", buf);
					continue;
				}

				if (strcmp(buf, "Line") == 0) {
					mixer->input_source[MIXER_INPUT_SOURCE_LINE] = i;
					g_log(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
							"\"%s\" is line "
							"source", buf);
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

	return 0;
}

int mixer_create(struct mixer **mixerp)
{
	static const char card[] = "default";
	struct mixer *mixer = NULL;
	struct pollfd *fds;
	GSource *source;
	int err;
	int i;

	source = g_source_new(&mixer_source_funcs, sizeof(*mixer));
	if (!source)
		return -ENOMEM;

	mixer = (struct mixer *)source;
	mixer->timeout = 250;

	for (i = 0; i < MIXER_INPUT_SOURCE_MAX; i++)
		mixer->input_source[i] = -1;

	err = snd_mixer_open(&mixer->mixer, 0);
	if (err < 0) {
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_ERROR, "failed to open audio "
				"mixer: %s", snd_strerror(err));
		goto free;
	}

	snd_mixer_set_callback(mixer->mixer, mixer_callback);

	err = snd_mixer_attach(mixer->mixer, card);
	if (err < 0) {
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_ERROR, "failed to attach "
				"audio mixer to card: %s", snd_strerror(err));
		goto close;
	}

	err = snd_mixer_selem_register(mixer->mixer, NULL, NULL);
	if (err < 0) {
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_ERROR, "failed to register "
				"elements: %s", snd_strerror(err));
		goto close;
	}

	err = snd_mixer_load(mixer->mixer);
	if (err < 0) {
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_ERROR, "failed to load "
				"mixer: %s", snd_strerror(err));
		goto close;
	}

	err = mixer_probe(mixer);
	if (err < 0) {
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_ERROR, "failed to probe "
				"mixer elements: %s", snd_strerror(err));
		goto close;
	}

	err = snd_mixer_poll_descriptors_count(mixer->mixer);
	if (err < 0) {
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_ERROR, "failed to get poll "
				"descriptor count: %s", snd_strerror(err));
		goto close;
	}

	mixer->num_fds = err;

	mixer->fds = g_new0(GPollFD, mixer->num_fds);
	if (!mixer->fds) {
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_ERROR, "failed to allocate "
				"poll descriptors\n");
		goto close;
	}

	fds = (struct pollfd *)mixer->fds;

	err = snd_mixer_poll_descriptors(mixer->mixer, fds, mixer->num_fds);
	if (err < 0) {
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_ERROR, "failed to get poll "
				"descriptors: %s", snd_strerror(err));
		goto free_fds;
	}

	for (i = 0; i < mixer->num_fds; i++)
		g_source_add_poll(source, &mixer->fds[i]);

	*mixerp = mixer;
	return 0;

free_fds:
	g_free(mixer->fds);
close:
	snd_mixer_close(mixer->mixer);
free:
	g_free(source);
	return err;
}

GSource *mixer_get_source(struct mixer *mixer)
{
	return mixer ? &mixer->source : NULL;
}

int mixer_set_volume(struct mixer *mixer, unsigned short control, unsigned int volume)
{
	struct mixer_element *element;
	long value = volume;
	const char *name;
	int err;

	if (control >= MIXER_CONTROL_MAX)
		return -EINVAL;

	element = mixer->elements[control];
	if (!element || !element->element)
		return -ENODEV;

	name = snd_mixer_selem_get_name(element->element);

	if (!element->ops || !element->ops->set_volume)
		return -ENOSYS;

	err = scale_volume(element, &value);
	if (err < 0) {
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_ERROR, "failed to scale "
				"volume for control %s: %s", name,
				snd_strerror(err));
		return err;
	}

	err = element->ops->set_volume(element->element, value);
	if (err < 0) {
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_ERROR, "failed to set volume "
				"for control %s: %s", name,
				snd_strerror(err));
		return err;
	}

	return 0;
}

int mixer_get_volume(struct mixer *mixer, unsigned short control, unsigned int *volumep)
{
	struct mixer_element *element;
	const char *name;
	long value = 0;
	int err;

	if (control >= MIXER_CONTROL_MAX)
		return -EINVAL;

	element = mixer->elements[control];
	if (!element || !element->element)
		return -ENODEV;

	name = snd_mixer_selem_get_name(element->element);

	if (!element->ops || !element->ops->get_volume)
		return -ENOSYS;

	err = element->ops->get_volume(element->element, &value);
	if (err < 0) {
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_ERROR, "failed to get "
				"volume for control %s: %s", name,
				snd_strerror(err));
		return err;
	}

	err = normalize_volume(element, &value);
	if (err < 0) {
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_ERROR, "failed to normalize "
				"volume for control %s: %s", name,
				snd_strerror(err));
		return err;
	}

	if (volumep)
		*volumep = value;

	return 0;
}

int mixer_set_mute(struct mixer *mixer, unsigned short control, bool mute)
{
	struct mixer_element *element;
	const char *name;
	int err;

	if (control >= MIXER_CONTROL_MAX)
		return -EINVAL;

	element = mixer->elements[control];
	if (!element || !element->element)
		return -ENODEV;

	name = snd_mixer_selem_get_name(element->element);

	if (!element->ops || !element->ops->set_switch)
		return -ENOSYS;

	err = element->ops->set_switch(element->element, !mute);
	if (err < 0)
		return err;

	return 0;
}

int mixer_is_muted(struct mixer *mixer, unsigned short control, bool *mutep)
{
	struct mixer_element *element;
	int value = 0;
	int err;

	if (control >= MIXER_CONTROL_MAX)
		return -EINVAL;

	element = mixer->elements[control];
	if (!element || !element->element)
		return -ENODEV;

	if (!element->ops || !element->ops->get_switch)
		return -ENOSYS;

	err = element->ops->get_switch(element->element, &value);
	if (err < 0)
		return err;

	if (mutep)
		*mutep = !value;

	return 0;
}

int mixer_set_input_source(struct mixer *mixer, enum mixer_input_source source)
{
	unsigned int i;
	int err = 0;

	if (source >= MIXER_INPUT_SOURCE_MAX)
		return -EINVAL;

	for (i = 0; i <= SND_MIXER_SCHN_LAST; i++) {
		if (mixer->input_source_bits & (1 << i)) {
			err = snd_mixer_selem_set_enum_item(mixer->input,
					i, mixer->input_source[source]);
			if (err < 0)
				break;
		}
	}

	return err;
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

static const unsigned int num_samples = 1024;

static int setup_pcm(snd_pcm_t *pcm, unsigned int rate, unsigned int channels,
		snd_pcm_format_t format)
{
	snd_pcm_hw_params_t *params = NULL;
	int err;

	err = snd_pcm_hw_params_malloc(&params);
	if (err < 0)
		return err;

	err = snd_pcm_hw_params_any(pcm, params);
	if (err < 0)
		return err;

	err = snd_pcm_hw_params_set_access(pcm, params, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (err < 0)
		return err;

	err = snd_pcm_hw_params_set_format(pcm, params, format);
	if (err < 0)
		return err;

	err = snd_pcm_hw_params_set_rate_near(pcm, params, &rate, 0);
	if (err < 0)
		return err;

	err = snd_pcm_hw_params_set_channels(pcm, params, channels);
	if (err < 0)
		return err;

	err = snd_pcm_hw_params(pcm, params);
	if (err < 0)
		return err;

	snd_pcm_hw_params_free(params);
	return 0;
}

static int create_playback(snd_pcm_t **playback, unsigned int rate,
		unsigned int channels, snd_pcm_format_t format)
{
	snd_pcm_sw_params_t *params;
	snd_pcm_uframes_t boundary;
	int err;

	err = snd_pcm_open(playback, "default", SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
	if (err < 0)
		return err;

	err = setup_pcm(*playback, rate, channels, format);
	if (err < 0)
		return err;

	err = snd_pcm_sw_params_malloc(&params);
	if (err < 0)
		return err;

	err = snd_pcm_sw_params_current(*playback, params);
	if (err < 0) {
		snd_pcm_sw_params_free(params);
		return err;
	}

	err = snd_pcm_sw_params_get_boundary(params, &boundary);
	if (err < 0) {
		snd_pcm_sw_params_free(params);
		return err;
	}

	err = snd_pcm_sw_params_set_start_threshold(*playback, params, num_samples * 2);
	if (err < 0) {
		snd_pcm_sw_params_free(params);
		return err;
	}

	err = snd_pcm_sw_params_set_stop_threshold(*playback, params, boundary);
	if (err < 0) {
		snd_pcm_sw_params_free(params);
		return err;
	}

	err = snd_pcm_sw_params_set_silence_size(*playback, params, boundary);
	if (err < 0) {
		snd_pcm_sw_params_free(params);
		return err;
	}

	err = snd_pcm_sw_params_set_avail_min(*playback, params, num_samples * 2);
	if (err < 0) {
		snd_pcm_sw_params_free(params);
		return err;
	}

	snd_pcm_sw_params_free(params);

	return snd_pcm_prepare(*playback);
}

static int create_capture(snd_pcm_t **capture, unsigned int rate,
		unsigned int channels, snd_pcm_format_t format)
{
	int err;

	err = snd_pcm_open(capture, "default", SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);
	if (err < 0)
		return err;

	err = setup_pcm(*capture, rate, channels, format);
	if (err < 0)
		return err;

	return snd_pcm_prepare(*capture);
}

static gpointer loopback_thread(gpointer data)
{
	struct mixer *mixer = data;
	snd_pcm_t *playback = NULL;
	snd_pcm_t *capture = NULL;
	gboolean written = TRUE;
	void *samples = NULL;
	int err;

	err = create_playback(&playback, 44100, 2, SND_PCM_FORMAT_S16_LE);
	if (err < 0) {
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_ERROR, "create_playback(): %s",
				snd_strerror(err));
		return NULL;
	}

	err = create_capture(&capture, 44100, 2, SND_PCM_FORMAT_S16_LE);
	if (err < 0) {
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_ERROR, "create_capture(): %s",
				snd_strerror(err));
		goto close_playback;
	}

	samples = g_new(short, num_samples * 2);
	if (!samples)
		goto close_capture;

	while (mixer->loop) {
		if (written) {
			err = snd_pcm_readi(capture, samples, num_samples);
			if (err < 0) {
				if (err == -EAGAIN) {
					snd_pcm_wait(capture, 100);
					continue;
				}

				g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "snd_pcm_readi(): %s",
						snd_strerror(err));
				break;
			}

			written = FALSE;
		}

		err = snd_pcm_writei(playback, samples, num_samples);
		if (err < 0) {
			if (err == -EAGAIN)
				continue;

			if (err == -EPIPE) {
				g_log(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "buffer underrun");
				snd_pcm_prepare(playback);
				continue;
			}

			g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "snd_pcm_writei(): %s",
					snd_strerror(err));
			mixer->loop = FALSE;
		}

		written = TRUE;
	}

	g_free(samples);

close_capture:
	snd_pcm_close(capture);
close_playback:
	snd_pcm_close(playback);
	return NULL;
}

int mixer_loopback_enable(struct mixer *mixer, bool enable)
{
	if (enable) {
		if (mixer->thread)
			return -EBUSY;

		mixer->loop = TRUE;

		mixer->thread = g_thread_create(loopback_thread, mixer, TRUE, NULL);
		if (!mixer->thread)
			return -ENOMEM;
	} else {
		if (!mixer->thread)
			return -ESRCH;

		mixer->loop = FALSE;
		g_thread_join(mixer->thread);
		mixer->thread = NULL;
	}

	return 0;
}

int mixer_loopback_is_enabled(struct mixer *mixer, bool *enabled)
{
	if (!mixer || !enabled)
		return -EINVAL;

	*enabled = (mixer->thread != NULL) && mixer->loop;
	return 0;
}
