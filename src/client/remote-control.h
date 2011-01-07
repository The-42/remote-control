/*
 * Copyright (C) 2010-2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef REMOTE_CONTROL_H
#define REMOTE_CONTROL_H 1

#include "remote-control-stub.h"

enum medcom_event {
	MEDCOM_EVENT_CARD,
	MEDCOM_EVENT_MODEM,
	MEDCOM_EVENT_VOIP,
	MEDCOM_EVENT_MAX,
};

typedef void (*medcom_event_handler_t)(uint32_t type, void *data);

struct medcom_client;

#ifdef __cplusplus
extern "C" {
#endif

int medcom_init(struct medcom_client **clientp, const char *hostname,
		const char *service);
int medcom_exit(struct medcom_client *client);

int medcom_register_event_handler(struct medcom_client *client,
		enum medcom_event queue, medcom_event_handler_t handler,
		void *data);
int medcom_unregister_event_handler(struct medcom_client *client,
		enum medcom_event queue, medcom_event_handler_t handler);

int32_t medcom_mixer_set_volume(void *priv, enum medcom_mixer_control control, uint8_t volume);
int32_t medcom_mixer_get_volume(void *priv, enum medcom_mixer_control control, uint8_t *volumep);
int32_t medcom_mixer_set_mute(void *priv, enum medcom_mixer_control control, bool mute);
int32_t medcom_mixer_get_mute(void *priv, enum medcom_mixer_control control, bool *mutep);
int32_t medcom_mixer_set_input_source(void *priv, enum medcom_mixer_input_source source);
int32_t medcom_mixer_get_input_source(void *priv, enum medcom_mixer_input_source *sourcep);

int32_t medcom_backlight_enable(void *priv, uint32_t flags);
int32_t medcom_backlight_get(void *priv, uint8_t *brightness);
int32_t medcom_backlight_set(void *priv, uint8_t brightness);

int32_t medcom_media_player_start(void *priv);
int32_t medcom_media_player_stop(void *priv);
int32_t medcom_media_player_is_running(void *priv, bool *running);
int32_t medcom_media_player_get_stream(void *priv, char **url);
int32_t medcom_media_player_set_stream(void *priv, const char *url);
int32_t medcom_media_player_set_output_window(void *priv, uint16_t x, uint16_t y, uint16_t width, uint16_t height);

int32_t medcom_voip_login(void *priv, struct medcom_voip_account *account);
int32_t medcom_voip_logout(void *priv);
int32_t medcom_voip_connect_to(void *priv, const char *uri);
int32_t medcom_voip_accept_incoming(void *priv, char **uri);
int32_t medcom_voip_disconnect(void *priv);
int32_t medcom_voip_still_logged_in(void *priv, bool *status);

int32_t medcom_card_enable(void *priv, bool enable);
int32_t medcom_card_reset(void *priv);
int32_t medcom_card_get_type(void *priv, enum medcom_card_type *type);
int32_t medcom_card_read(void *priv, off_t offset, void *buffer, size_t size);
int32_t medcom_card_write(void *priv, off_t offset, const void *buffer, size_t size);

int32_t medcom_irq_enable(void *priv, uint8_t virtkey);
int32_t medcom_irq_get_mask(void *priv, uint32_t *mask);
int32_t medcom_irq_get_info(void *priv, enum medcom_irq_source source, uint32_t *info);

#ifdef __cplusplus
};
#endif

#endif /* REMOTE_CONTROL_H */
