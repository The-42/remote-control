/*
 * Copyright (C) 2010-2012 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <netdb.h>
#include <glib.h>

#include "remote-control-stub.h"
#include "remote-control.h"

struct remote_control {
	struct event_manager *event_manager;
	struct gpio_backend *gpio;
	struct audio *audio;
	struct backlight *backlight;
	struct cursor_movement *cursor_movement;
	struct media_player *player;
	struct sound_manager *sound;
	struct smartcard *smartcard;
	struct modem_manager *modem;
	struct voip *voip;
	struct mixer *mixer;
	struct net_udp *net_udp;
	struct lldp_monitor *lldp;
	struct task_manager *task_manager;
	struct tuner *tuner;
	struct handset *handset;
	struct app_watchdog *watchdog;

	GSource *source;
};

static gboolean remote_control_source_prepare(GSource *source, gint *timeout)
{
	if (timeout)
		*timeout = -1;

	return FALSE;
}

static gboolean remote_control_source_check(GSource *source)
{
	return FALSE;
}

static gboolean remote_control_source_dispatch(GSource *source, GSourceFunc callback, gpointer user_data)
{
	gboolean ret = TRUE;

	if (callback)
		ret = callback(user_data);

	return ret;
}

static void remote_control_source_finalize(GSource *source)
{
}

static GSourceFuncs remote_control_source_funcs = {
	.prepare = remote_control_source_prepare,
	.check = remote_control_source_check,
	.dispatch = remote_control_source_dispatch,
	.finalize = remote_control_source_finalize,
};

int remote_control_create(struct remote_control **rcp, GKeyFile *config)
{
	struct remote_control *rc;
	GSource *source;
	int err;

	if (!rcp)
		return -EINVAL;

	rc = g_new0(struct remote_control, 1);

	rc->source = g_source_new(&remote_control_source_funcs, sizeof(GSource));
	if (!rc->source) {
		g_critical("g_source_new() failed");
		return -ENOMEM;
	}

	err = lldp_monitor_create(&rc->lldp, config);
	if (err < 0) {
		g_critical("lldp_monitor_create(): %s", strerror(-err));
		return err;
	}

	source = lldp_monitor_get_source(rc->lldp);
	if (source) {
		g_source_add_child_source(rc->source, source);
		g_source_unref(source);
	}

	err = event_manager_create(&rc->event_manager);
	if (err < 0) {
		g_critical("event_manager_create(): %s", strerror(-err));
		return err;
	}

	err = backlight_create(&rc->backlight);
	if (err < 0) {
		g_critical("backlight_create(): %s", strerror(-err));
		return err;
	}

	err = cursor_movement_create(&rc->cursor_movement);
	if (err < 0) {
		g_critical("cursor_movement_create(): %s", strerror(-err));
		return err;
	}

	err = media_player_create(&rc->player, config);
	if (err < 0) {
		g_critical("media_player_create(): %s", strerror(-err));
		return err;
	}

	err = audio_create(&rc->audio, rc, config);
	if (err < 0) {
		g_critical("audio_create(): %s", strerror(-err));
		return err;
	}

	err = sound_manager_create(&rc->sound, rc->audio, config);
	if (err < 0) {
		g_critical("sound_manager_create(): %s", strerror(-err));
		return err;
	}

	err = smartcard_create(&rc->smartcard, rc, config);
	if (err < 0) {
		g_critical("smartcard_create(): %s", strerror(-err));
		return err;
	}

	err = modem_manager_create(&rc->modem, rc, config);
	if (err < 0) {
		g_critical("modem_manager_create(): %s", strerror(-err));
		return err;
	}

	source = modem_manager_get_source(rc->modem);
	if (source) {
		g_source_add_child_source(rc->source, source);
		g_source_unref(source);
	}

	err = voip_create(&rc->voip, rc, config);
	if (err < 0) {
		g_critical("voip_create(): %s", strerror(-err));
		return err;
	}

	source = voip_get_source(rc->voip);
	if (source) {
		g_source_add_child_source(rc->source, source);
		g_source_unref(source);
	}

	err = net_udp_create(&rc->net_udp);
	if (err < 0) {
		g_critical("net_udp_create(): %s", strerror(-err));
		return err;
	}

	err = task_manager_create(&rc->task_manager);
	if (err < 0) {
		g_critical("task_manager_create(): %s", strerror(-err));
		return err;
	}

	err = tuner_create(&rc->tuner);
	if (err < 0) {
		g_critical("tuner_create(): %s", strerror(-err));
		return err;
	}

	err = handset_create(&rc->handset, rc);
	if (err < 0) {
		g_critical("handset_create(): %s", strerror(-err));
		return err;
	}

	err = mixer_create(&rc->mixer);
	if (err < 0) {
		g_critical("mixer_create(): %s", strerror(-err));
		return err;
	}

	source = mixer_get_source(rc->mixer);
	if (source) {
		g_source_add_child_source(rc->source, source);
		g_source_unref(source);
	}

	err = gpio_backend_create(&rc->gpio, rc->event_manager, config);
	if (err < 0) {
		g_critical("gpio_backend_create(): %s", strerror(-err));
		return err;
	}

	source = gpio_backend_get_source(rc->gpio);
	if (source) {
		g_source_add_child_source(rc->source, source);
		g_source_unref(source);
	}

	err = usb_handset_create(rc);
	if (err < 0) {
		g_critical("usb_handset_create(): %s", strerror(-err));
		return err;
	}

	err = app_watchdog_create(&rc->watchdog, config);
	if (err < 0) {
		g_critical("app_watchdog_create(): %s", strerror(-err));
		return err;
	}

	*rcp = rc;
	return 0;
}

GSource *remote_control_get_source(struct remote_control *rc)
{
	return rc->source;
}

int remote_control_free(struct remote_control *rc)
{
	if (!rc)
		return -EINVAL;

	handset_free(rc->handset);
	tuner_free(rc->tuner);
	task_manager_free(rc->task_manager);
	net_udp_free(rc->net_udp);
	voip_free(rc->voip);
	smartcard_free(rc->smartcard);
	sound_manager_free(rc->sound);
	media_player_free(rc->player);
	cursor_movement_free(rc->cursor_movement);
	backlight_free(rc->backlight);
	audio_free(rc->audio);
	app_watchdog_free(rc->watchdog);
	gpio_backend_free(rc->gpio);
	event_manager_free(rc->event_manager);
	lldp_monitor_free(rc->lldp);
	g_free(rc);

	return 0;
}

struct event_manager *remote_control_get_event_manager(struct remote_control *rc)
{
	return rc ? rc->event_manager : NULL;
}

struct audio *remote_control_get_audio(struct remote_control *rc)
{
	return rc ? rc->audio : NULL;
}

struct backlight *remote_control_get_backlight(struct remote_control *rc)
{
	return rc ? rc->backlight : NULL;
}

struct cursor_movement *remote_control_get_cursor_movement(struct remote_control *rc)
{
	return rc ? rc->cursor_movement : NULL;
}

struct media_player *remote_control_get_media_player(struct remote_control *rc)
{
	return rc ? rc->player : NULL;
}

struct sound_manager *remote_control_get_sound_manager(struct remote_control *rc)
{
	return rc ? rc->sound : NULL;
}

struct smartcard *remote_control_get_smartcard(struct remote_control *rc)
{
	return rc ? rc->smartcard : NULL;
}

struct modem_manager *remote_control_get_modem_manager(struct remote_control *rc)
{
	return rc ? rc->modem : NULL;
}

struct voip *remote_control_get_voip(struct remote_control *rc)
{
	return rc ? rc->voip : NULL;
}

struct mixer *remote_control_get_mixer(struct remote_control *rc)
{
	return rc ? rc->mixer : NULL;
}

struct net_udp *remote_control_get_net_udp(struct remote_control *rc)
{
	return rc ? rc->net_udp : NULL;
}

struct lldp_monitor *remote_control_get_lldp_monitor(struct remote_control *rc)
{
	return rc ? rc->lldp : NULL;
}

struct task_manager *remote_control_get_task_manager(struct remote_control *rc)
{
	return rc ? rc->task_manager : NULL;
}

struct tuner *remote_control_get_tuner(struct remote_control *rc)
{
	return rc ? rc->tuner : NULL;
}

struct handset *remote_control_get_handset(struct remote_control *rc)
{
	return rc ? rc->handset : NULL;
}

struct gpio_backend *remote_control_get_gpio_backend(struct remote_control *rc)
{
	return rc ? rc->gpio : NULL;
}

struct app_watchdog *remote_control_get_watchdog(struct remote_control *rc)
{
	return rc ? rc->watchdog : NULL;
}
