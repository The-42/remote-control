/*
 * Copyright (C) 2010 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <string.h>

#include "cli.h"

const char *true_values[] = { "true", "on", "yes", "enable" };
const char *false_values[] = { "false", "off", "no", "disable" };

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static int parse_bool(const char *string, bool *res)
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

static enum medcom_mixer_control parse_mixer_control(const char *control)
{
	enum medcom_mixer_control ret = MIXER_CONTROL_UNKNOWN;

	if (strcasecmp(control, "master") == 0)
		ret = MIXER_CONTROL_PLAYBACK_MASTER;

	if (strcasecmp(control, "pcm") == 0)
		ret = MIXER_CONTROL_PLAYBACK_PCM;

	if (strcasecmp(control, "headset") == 0)
		ret = MIXER_CONTROL_PLAYBACK_HEADSET;

	if (strcasecmp(control, "speaker") == 0)
		ret = MIXER_CONTROL_PLAYBACK_SPEAKER;

	if (strcasecmp(control, "handset") == 0)
		ret = MIXER_CONTROL_PLAYBACK_HANDSET;

	if (strcasecmp(control, "capture") == 0)
		ret = MIXER_CONTROL_CAPTURE_MASTER;

	return ret;
}

/*
 * "help" command
 */
static const struct shcmd_info info_help[] = {
	{ "help", gettext_noop("print help") },
	{ "desc", gettext_noop("Prints global or command specific help.") },
	{ NULL, NULL },
};

static const struct shcmd_opt_def opts_help[] = {
	{ "command", SHCMD_OT_DATA, 0, gettext_noop("name of command") },
	{ NULL, 0, 0, NULL },
};

static int cmd_help(struct shctl *ctl, const struct shcmd *cmd)
{
	struct cli *cli = shctl_priv(ctl);
	char *name = NULL;

	shcmd_get_opt_string(cmd, "command", &name);
	if (!name) {
		const struct shcmd_def *def;

		shctl_log(ctl, 0, "Commands:\n\n");

		for (def = cli->commands; def && def->name; def++) {
			const char *info = shcmd_def_get_info(def, "help");
			shctl_log(ctl, 0, "    %-25s %s\n", def->name, info);
		}

		shctl_log(ctl, 0, "\n");
	} else {
		return shctl_def_help(ctl, name);
	}

	return 0;
}

/*
 * "mixer-volume" command
 */
static const struct shcmd_info info_mixer_volume[] = {
	{ "help", gettext_noop("set audio volume") },
	{ "desc", gettext_noop("Sets audio volume.") },
	{ NULL, NULL },
};

static const struct shcmd_opt_def opts_mixer_volume[] = {
	{ "control", SHCMD_OT_DATA, 0, gettext_noop("audio control") },
	{ "volume", SHCMD_OT_DATA, 0, gettext_noop("volume") },
	{ NULL, 0, 0, NULL },
};

static int cmd_mixer_volume(struct shctl *ctl, const struct shcmd *cmd)
{
	struct cli *cli = shctl_priv(ctl);
	enum medcom_mixer_control control = MIXER_CONTROL_UNKNOWN;
	char *data = NULL;
	int err;

	err = shcmd_get_opt_string(cmd, "control", &data);
	if (err < 0) {
		shctl_log(ctl, 0, "control not specified\n");
		return -EINVAL;
	}

	control = parse_mixer_control(data);

	err = shcmd_get_opt_string(cmd, "volume", &data);
	if (err < 0) {
		uint8_t volume = 0;

		err = medcom_mixer_get_volume(cli->client, control, &volume);
		if (err < 0)
			return err;

		shctl_log(ctl, 0, "volume: %u\n", volume);
	} else {
		uint8_t volume = 0;

		/* TODO: parse volume */

		err = medcom_mixer_set_volume(cli->client, control, volume);
		if (err < 0)
			return err;
	}

	return 0;
}

/*
 * "mixer-mute" command
 */
static const struct shcmd_info info_mixer_mute[] = {
	{ "help", gettext_noop("mute or unmute audio control") },
	{ "desc", gettext_noop("Mutes or unmutes an audio control.") },
	{ NULL, NULL },
};

static const struct shcmd_opt_def opts_mixer_mute[] = {
	{ "control", SHCMD_OT_DATA, 0, gettext_noop("audio control") },
	{ "mute", SHCMD_OT_DATA, 0, gettext_noop("mute") },
	{ NULL, 0, 0, NULL },
};

static int cmd_mixer_mute(struct shctl *ctl, const struct shcmd *cmd)
{
	struct cli *cli = shctl_priv(ctl);
	enum medcom_mixer_control control = MIXER_CONTROL_UNKNOWN;
	char *data = NULL;
	int err;

	err = shcmd_get_opt_string(cmd, "control", &data);
	if (err < 0) {
		shctl_log(ctl, 0, "control not specified\n");
		return -EINVAL;
	}

	control = parse_mixer_control(data);

	err = shcmd_get_opt_string(cmd, "mute", &data);
	if (err < 0) {
		bool mute = false;

		err = medcom_mixer_get_mute(cli->client, control, &mute);
		if (err < 0)
			return err;

		shctl_log(ctl, 0, "muted: %s\n", mute ? "yes" : "no");
	} else {
		bool mute = false;

		err = parse_bool(data, &mute);
		if (err < 0) {
			shctl_log(ctl, 0, "Invalid boolean value '%s'.\n",
					data);
			return -EINVAL;
		}

		err = medcom_mixer_set_mute(cli->client, control, mute);
		if (err < 0)
			return err;
	}

	return 0;
}

/*
 * "backlight-power" command
 */
static const struct shcmd_info info_backlight_power[] = {
	{ "help", gettext_noop("control backlight power") },
	{ "desc", gettext_noop("Controls the display's backlight power.") },
	{ NULL, NULL },
};

static const struct shcmd_opt_def opts_backlight_power[] = {
	{ "power", SHCMD_OT_DATA, 0, gettext_noop("backlight power") },
	{ NULL, 0, 0, NULL },
};

static int cmd_backlight_power(struct shctl *ctl, const struct shcmd *cmd)
{
	struct cli *cli = shctl_priv(ctl);
	char *power = NULL;
	int err;

	err = shcmd_get_opt_string(cmd, "power", &power);
	if (err < 0) {
		/* TODO: implement backlight power query */
	} else {
		bool enable = false;

		err = parse_bool(power, &enable);
		if (err < 0) {
			shctl_log(ctl, 0, "Invalid boolean value '%s'.\n",
					power);
			return -EINVAL;
		}

		err = medcom_backlight_enable(cli->client, enable);
		if (err < 0)
			return err;
	}

	return 0;
}

/*
 * "backlight-brightness" command
 */
static const struct shcmd_info info_backlight_brightness[] = {
	{ "help", gettext_noop("control backlight brightness") },
	{ "desc", gettext_noop("Controls the display's backlight brightness.") },
	{ NULL, NULL },
};

static const struct shcmd_opt_def opts_backlight_brightness[] = {
	{ "brightness", SHCMD_OT_DATA, 0, gettext_noop("backlight brightness") },
	{ NULL, 0, 0, NULL },
};

static int cmd_backlight_brightness(struct shctl *ctl, const struct shcmd *cmd)
{
	struct cli *cli = shctl_priv(ctl);
	char *brightness = NULL;
	int err;

	err = shcmd_get_opt_string(cmd, "brightness", &brightness);
	if (err < 0) {
		uint8_t brightness = 0;

		err = medcom_backlight_get(cli->client, &brightness);
		if (err < 0)
			return err;

		shctl_log(ctl, 0, "brightness: %u\n", brightness);
	} else {
		unsigned long value = 0;
		char *end = NULL;

		value = strtoul(brightness, &end, 0);
		if (end == brightness) {
			shctl_log(ctl, 0, "Invalid numerical value '%s'.\n",
					brightness);
			return -EINVAL;
		}

		err = medcom_backlight_set(cli->client, value);
		if (err < 0)
			return err;
	}

	return 0;
}

/*
 * "media-player-uri" command
 */
static const struct shcmd_info info_media_player_uri[] = {
	{ "help", gettext_noop("set or get media stream URI") },
	{ "desc", gettext_noop("Sets or retrieves the media stream URI.") },
	{ NULL, NULL },
};

static const struct shcmd_opt_def opts_media_player_uri[] = {
	{ "uri", SHCMD_OT_DATA, 0, gettext_noop("media stream URI") },
	{ NULL, 0, 0, NULL },
};

static int cmd_media_player_uri(struct shctl *ctl, const struct shcmd *cmd)
{
	struct cli *cli = shctl_priv(ctl);
	char *uri = NULL;
	int err;

	err = shcmd_get_opt_string(cmd, "uri", &uri);
	if (err < 0) {
		err = medcom_media_player_get_stream(cli->client, &uri);
		if (err < 0)
			return err;

		shctl_log(ctl, 0, "URI: %s\n", uri);
	} else {
		err = medcom_media_player_set_stream(cli->client, uri);
		if (err < 0)
			return err;
	}

	return 0;
}

/*
 * "media-player-output" command
 */
static const struct shcmd_info info_media_player_output[] = {
	{ "help", gettext_noop("set or get media player output window") },
	{ "desc", gettext_noop("Sets or retrieves the media player's output window.") },
	{ NULL, NULL },
};

static const struct shcmd_opt_def opts_media_player_output[] = {
	{ "x", SHCMD_OT_DATA, 0, gettext_noop("X coordinate of output window") },
	{ "y", SHCMD_OT_DATA, 0, gettext_noop("Y-coordinate of output window") },
	{ "width", SHCMD_OT_DATA, 0, gettext_noop("width of output window") },
	{ "height", SHCMD_OT_DATA, 0, gettext_noop("height of output window") },
	{ NULL, 0, 0, NULL },
};

static int cmd_media_player_output(struct shctl *ctl, const struct shcmd *cmd)
{
	struct cli *cli = shctl_priv(ctl);
	char *coord = NULL;
	uint16_t x = UINT16_MAX;
	uint16_t y = UINT16_MAX;
	uint16_t width = UINT16_MAX;
	uint16_t height = UINT16_MAX;
	int err;

	err = shcmd_get_opt_string(cmd, "x", &coord);
	if (!err) {
		unsigned long value;
		char *end = NULL;

		value = strtoul(coord, &end, 0);
		if (end != coord)
			x = value;
	}

	err = shcmd_get_opt_string(cmd, "y", &coord);
	if (!err) {
		unsigned long value;
		char *end = NULL;

		value = strtoul(coord, &end, 0);
		if (end != coord)
			y = value;
	}

	err = shcmd_get_opt_string(cmd, "width", &coord);
	if (!err) {
		unsigned long value;
		char *end = NULL;

		value = strtoul(coord, &end, 0);
		if (end != coord)
			width = value;
	}

	err = shcmd_get_opt_string(cmd, "height", &coord);
	if (!err) {
		unsigned long value;
		char *end = NULL;

		value = strtoul(coord, &end, 0);
		if (end != coord)
			height = value;
	}

	if ((x == UINT16_MAX) && (y == UINT16_MAX) &&
	    (width == UINT16_MAX) && (height == UINT16_MAX)) {
	} else {
		err = medcom_media_player_set_output_window(cli->client,
				x, y, width, height);
		if (err < 0)
			return err;
	}

	return 0;
}

/*
 * "media-player-play" command
 */
static const struct shcmd_info info_media_player_play[] = {
	{ "help", gettext_noop("start playback") },
	{ "desc", gettext_noop("Starts playback of a given media stream.") },
	{ NULL, NULL },
};

static const struct shcmd_opt_def opts_media_player_play[] = {
	{ "uri", SHCMD_OT_DATA, 0, gettext_noop("media stream URI") },
	{ NULL, 0, 0, NULL },
};

static int cmd_media_player_play(struct shctl *ctl, const struct shcmd *cmd)
{
	struct cli *cli = shctl_priv(ctl);
	char *uri = NULL;
	int err;

	err = shcmd_get_opt_string(cmd, "uri", &uri);
	if (!err) {
		err = medcom_media_player_set_stream(cli->client, uri);
		if (err < 0)
			return err;
	}

	return medcom_media_player_start(cli->client);
}

/*
 * "media-player-stop" command
 */
static const struct shcmd_info info_media_player_stop[] = {
	{ "help", gettext_noop("stop playback") },
	{ "desc", gettext_noop("Stops playback of the current media stream.") },
	{ NULL, NULL },
};

static const struct shcmd_opt_def opts_media_player_stop[] = {
	{ NULL, 0, 0, NULL },
};

static int cmd_media_player_stop(struct shctl *ctl, const struct shcmd *cmd)
{
	struct cli *cli = shctl_priv(ctl);
	return medcom_media_player_stop(cli->client);
}

/*
 * "media-player-state" command
 */
static const struct shcmd_info info_media_player_state[] = {
	{ "help", gettext_noop("retrieve player state") },
	{ "desc", gettext_noop("Retrieves the current media player state.") },
	{ NULL, NULL },
};

static const struct shcmd_opt_def opts_media_player_state[] = {
	{ NULL, 0, 0, NULL },
};

static int cmd_media_player_state(struct shctl *ctl, const struct shcmd *cmd)
{
	struct cli *cli = shctl_priv(ctl);
	const char *state = "unknown";
	bool running = false;
	int32_t err;

	err = medcom_media_player_is_running(cli->client, &running);
	if (err < 0)
		return err;

	if (running)
		state = "playing";
	else
		state = "stopped";

	shctl_log(ctl, 0, "state: %s\n", state);
	return 0;
}

/*
 * "voip-login" command
 */
static const struct shcmd_info info_voip_login[] = {
	{ "help", gettext_noop("register VoIP") },
	{ "desc", gettext_noop("Registers the target with a VoIP server.") },
	{ NULL, NULL },
};

static const struct shcmd_opt_def opts_voip_login[] = {
	{ "server", SHCMD_OT_DATA, 0, gettext_noop("server hostname or address") },
	{ "port", SHCMD_OT_DATA, 0, gettext_noop("server port") },
	{ "username", SHCMD_OT_DATA, 0, gettext_noop("username") },
	{ "password", SHCMD_OT_DATA, 0, gettext_noop("password") },
	{ NULL, 0, 0, NULL },
};

static int cmd_voip_login(struct shctl *ctl, const struct shcmd *cmd)
{
	struct medcom_voip_account account;
	struct cli *cli = shctl_priv(ctl);
	char *arg = NULL;
	char *end = NULL;
	int err;

	err = shcmd_get_opt_string(cmd, "server", &account.server);
	if (err)
		account.server = NULL;
		//account.server = "sip-0002.mockup.avionic-design.de";

	err = shcmd_get_opt_string(cmd, "port", &arg);
	if (!err)
		account.port = strtoul(arg, &end, 0);
	else
		account.port = 0;
		//account.port = 5060;

	err = shcmd_get_opt_string(cmd, "username", &account.username);
	if (err)
		account.username = NULL;
		//account.username = "400";

	err = shcmd_get_opt_string(cmd, "password", &account.password);
	if (err)
		account.password = NULL;
		//account.password = "400";

	return medcom_voip_login(cli->client, &account);
}

/*
 * "voip-logout" command
 */
static const struct shcmd_info info_voip_logout[] = {
	{ "help", gettext_noop("unregister VoIP") },
	{ "desc", gettext_noop("Unregisters the target with a VoIP server.") },
	{ NULL, NULL },
};

static const struct shcmd_opt_def opts_voip_logout[] = {
	{ NULL, 0, 0, NULL },
};

static int cmd_voip_logout(struct shctl *ctl, const struct shcmd *cmd)
{
	struct cli *cli = shctl_priv(ctl);

	return medcom_voip_logout(cli->client);
}

/*
 * "voip-call" command
 */
static const struct shcmd_info info_voip_call[] = {
	{ "help", gettext_noop("make a VoIP call") },
	{ "desc", gettext_noop("Initiates a VoIP call.") },
	{ NULL, NULL },
};

static const struct shcmd_opt_def opts_voip_call[] = {
	{ "url", SHCMD_OT_DATA, 0, gettext_noop("callee URL") },
	{ NULL, 0, 0, NULL },
};

static int cmd_voip_call(struct shctl *ctl, const struct shcmd *cmd)
{
	struct cli *cli = shctl_priv(ctl);
	char *arg = NULL;
	int err;

	err = shcmd_get_opt_string(cmd, "url", &arg);

	return medcom_voip_connect_to(cli->client, arg);
}

/*
 * "voip-accept" command
 */
static const struct shcmd_info info_voip_accept[] = {
	{ "help", gettext_noop("accept incoming VoIP call") },
	{ "desc", gettext_noop("Accepts an incoming VoIP call.") },
	{ NULL, NULL },
};

static const struct shcmd_opt_def opts_voip_accept[] = {
	{ NULL, 0, 0, NULL },
};

static int cmd_voip_accept(struct shctl *ctl, const struct shcmd *cmd)
{
	struct cli *cli = shctl_priv(ctl);
	char *arg = NULL;
	int err;

	err = medcom_voip_accept_incoming(cli->client, &arg);
	if (err < 0)
		return err;

	printf("  caller:%s\n", arg);
	return err;
}

/*
 * "voip-terminate" command
 */
static const struct shcmd_info info_voip_terminate[] = {
	{ "help", gettext_noop("terminate a VoIP call") },
	{ "desc", gettext_noop("Terminates a VoIP call.") },
	{ NULL, NULL },
};

static const struct shcmd_opt_def opts_voip_terminate[] = {
	{ NULL, 0, 0, NULL },
};

static int cmd_voip_terminate(struct shctl *ctl, const struct shcmd *cmd)
{
	struct cli *cli = shctl_priv(ctl);
	return medcom_voip_disconnect(cli->client);
}

const struct shcmd_def cli_commands[] = {
	{ "help", cmd_help, opts_help, info_help },
	{ "mixer-volume", cmd_mixer_volume, opts_mixer_volume,
		info_mixer_volume },
	{ "mixer-mute", cmd_mixer_mute, opts_mixer_mute, info_mixer_mute },
	{ "backlight-power", cmd_backlight_power, opts_backlight_power,
		info_backlight_power },
	{ "backlight-brightness", cmd_backlight_brightness,
		opts_backlight_brightness, info_backlight_brightness },
	{ "media-player-uri", cmd_media_player_uri, opts_media_player_uri,
		info_media_player_uri },
	{ "media-player-output", cmd_media_player_output,
		opts_media_player_output, info_media_player_output },
	{ "media-player-play", cmd_media_player_play,
		opts_media_player_play, info_media_player_play },
	{ "media-player-stop", cmd_media_player_stop,
		opts_media_player_stop, info_media_player_stop },
	{ "media-player-state", cmd_media_player_state,
		opts_media_player_state, info_media_player_state },
	{ "voip-login", cmd_voip_login, opts_voip_login, info_voip_login },
	{ "voip-logout", cmd_voip_logout, opts_voip_logout, info_voip_logout },
	{ "voip-call", cmd_voip_call, opts_voip_call, info_voip_call },
	{ "voip-accept", cmd_voip_accept, opts_voip_accept, info_voip_accept },
	{ "voip-terminate", cmd_voip_terminate, opts_voip_terminate,
		info_voip_terminate },
	{ NULL, NULL, NULL, NULL },
};
