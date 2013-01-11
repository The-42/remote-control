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

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "remote-control-client.h"

const char *true_values[] = { "true", "on", "yes", "enable" };
const char *false_values[] = { "false", "off", "no", "disable" };

static const struct {
	enum remote_audio_state id;
	const char* name;
} audio_state_map[] = {
	{ REMOTE_AUDIO_STATE_UNKNOWN, "Unknown" },
	{ REMOTE_AUDIO_STATE_INACTIVE, "Inactive" },
	{ REMOTE_AUDIO_STATE_HIFI_PLAYBACK_SPEAKER, "Hifi-Playback-Speaker" },
	{ REMOTE_AUDIO_STATE_HIFI_PLAYBACK_HEADSET, "Hifi-Playback-Headset" },
	{ REMOTE_AUDIO_STATE_VOICECALL_HANDSET, "VoiceCall-Handset" },
	{ REMOTE_AUDIO_STATE_VOICECALL_HEADSET, "VoiceCall-Headset" },
	{ REMOTE_AUDIO_STATE_VOICECALL_SPEAKER, "VoiceCall-Speaker" },
	{ REMOTE_AUDIO_STATE_VOICECALL_IP_HANDSET, "VoiceCallIP-Handset" },
	{ REMOTE_AUDIO_STATE_VOICECALL_IP_HEADSET, "VoiceCallIP-Headset" },
	{ REMOTE_AUDIO_STATE_VOICECALL_IP_SPEAKER, "VoiceCallIP-Speaker" },
};

static const struct {
	enum remote_mixer_control id;
	const char *name;
} mixer_control_map[] = {
	{ REMOTE_MIXER_CONTROL_PLAYBACK_MASTER, "master" },
	{ REMOTE_MIXER_CONTROL_PLAYBACK_PCM, "pcm" },
	{ REMOTE_MIXER_CONTROL_PLAYBACK_HEADSET, "headset" },
	{ REMOTE_MIXER_CONTROL_PLAYBACK_SPEAKER, "speaker" },
	{ REMOTE_MIXER_CONTROL_PLAYBACK_HANDSET, "handset" },
	{ REMOTE_MIXER_CONTROL_CAPTURE_MASTER, "capture" },
};

static const struct {
	enum remote_mixer_input_source id;
	const char *name;
} mixer_input_map[] = {
	{ REMOTE_MIXER_INPUT_SOURCE_HEADSET, "headset" },
	{ REMOTE_MIXER_INPUT_SOURCE_HANDSET, "handset" },
	{ REMOTE_MIXER_INPUT_SOURCE_LINE, "line" },
};

int parse_bool(const char *string, bool *res)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(true_values); i++) {
		if (strcasecmp(string, true_values[i]) == 0) {
			*res = true;
			return 0;
		}
	}

	for (i = 0; i < ARRAY_SIZE(false_values); i++) {
		if (strcasecmp(string, false_values[i]) == 0) {
			*res = false;
			return 0;
		}
	}

	return -EILSEQ;
}

enum remote_audio_state parse_audio_state(const char *state)
{
	enum remote_audio_state ret = REMOTE_AUDIO_STATE_UNKNOWN;
	unsigned int i;
	int value;

	if (!state)
		goto out;

	if (sscanf(state, "%d", &value) != 1)
		goto out;

	for (i = 0; i < ARRAY_SIZE(audio_state_map); i++) {
		if (audio_state_map[i].id == value) {
			ret = audio_state_map[i].id;
			goto out;
		}
	}

	for (i = 0; i < ARRAY_SIZE(audio_state_map); i++) {
		if (strcasecmp(audio_state_map[i].name, state) == 0) {
			ret = audio_state_map[i].id;
			break;
		}
	}

out:
	return ret;
}

int audio_state_name(enum remote_audio_state state, char *buffer, size_t size)
{
	unsigned int i;
	int ret = 0;

	for (i = 0; i < ARRAY_SIZE(audio_state_map); i++) {
		if (audio_state_map[i].id == state) {
			ret = snprintf(buffer, size, audio_state_map[i].name);
			break;
		}
	}

	if (ret == 0)
		ret = snprintf(buffer, size, "unknown");

	if (ret < 0)
		ret = -errno;

	return ret;
}

void audio_state_print_names()
{
	unsigned int i;

	for (i = 1; i < ARRAY_SIZE(audio_state_map); i++) {
		printf("   %.02d: %s\n", audio_state_map[i].id,
			audio_state_map[i].name);
	}
}

enum remote_mixer_control parse_mixer_control(const char *control)
{
	enum remote_mixer_control ret = REMOTE_MIXER_CONTROL_UNKNOWN;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(mixer_control_map); i++) {
		if (strcasecmp(control, mixer_control_map[i].name) == 0) {
			ret = mixer_control_map[i].id;
			break;
		}
	}

	return ret;
}

int mixer_input_source_name(enum remote_mixer_input_source source,
		char *buffer, size_t size)
{
	unsigned int i;
	int ret = 0;

	for (i = 0; i < ARRAY_SIZE(mixer_input_map); i++) {
		if (mixer_input_map[i].id == source) {
			ret = snprintf(buffer, size, mixer_input_map[i].name);
			break;
		}
	}

	if (ret == 0)
		ret = snprintf(buffer, size, "unknown");

	if (ret < 0)
		ret = -errno;

	return ret;
}

enum remote_mixer_input_source parse_mixer_input_source(const char *source)
{
	enum remote_mixer_input_source ret = REMOTE_MIXER_INPUT_SOURCE_UNKNOWN;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(mixer_input_map); i++) {
		if (strcasecmp(source, mixer_input_map[i].name) == 0) {
			ret = mixer_input_map[i].id;
			break;
		}
	}

	return ret;
}

void print_hex_dump(FILE *fp, const char *prefix, int prefix_type,
		int rowsize, int groupsize, const void *buf, size_t len,
		bool ascii)
{
	const uint8_t *ptr = buf;
	size_t i;
	size_t j;

	for (j = 0; j < len; j += rowsize) {
		switch (prefix_type) {
		case DUMP_PREFIX_ADDRESS:
			fprintf(fp, "%s%p: ", prefix, ptr + j);
			break;

		case DUMP_PREFIX_OFFSET:
			fprintf(fp, "%s%.8zx: ", prefix, j);
			break;

		default:
			fprintf(fp, "%s", prefix);
			break;
		}

		for (i = 0; i < rowsize; i++) {
			if ((j + i) < len)
				fprintf(fp, "%s%02x", i ? " " : "", ptr[j + i]);
			else
				fprintf(fp, "%s  ", i ? " " : "");
		}

		fprintf(fp, " | ");

		for (i = 0; i < rowsize; i++) {
			if ((j + i) < len) {
				if (isprint(ptr[j + i]))
					fprintf(fp, "%c", ptr[j + i]);
				else
					fprintf(fp, ".");
			} else {
				fprintf(fp, " ");
			}
		}

		fprintf(fp, " |\n");
	}
}

char **slice_strdup(char *argv[], int start, int end)
{
	char **slice = NULL;

	if (argv && (end > start)) {
		int count = end - start;
		int i;

		slice = calloc(count + 1, sizeof(char *));
		if (!slice)
			return NULL;

		for (i = 0; i < count; i++)
			slice[i] = strdup(argv[start + i]);

		slice[count] = NULL;
	}

	return slice;
}

void strfreev(char *strv[])
{
	if (strv) {
		char **ptr = strv;
		while (*ptr) {
			free(*ptr);
			ptr++;
		}

		free(strv);
	}
}

bool shell_str_needs_escape(const char *s)
{
	while (*s && !isspace(*s))
		s++;

	return *s != '\0';
}
