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

int medcom_init(struct medcom_client **clientp, const char *hostname,
		const char *service);
int medcom_exit(struct medcom_client *client);

int medcom_register_event_handler(struct medcom_client *client,
		enum medcom_event queue, medcom_event_handler_t handler,
		void *data);

int32_t medcom_backlight_enable(void *priv, uint32_t flags);
int32_t medcom_backlight_get(void *priv, uint8_t *brightness);
int32_t medcom_backlight_set(void *priv, uint8_t brightness);

int32_t medcom_media_player_start(void *priv);
int32_t medcom_media_player_stop(void *priv);
int32_t medcom_media_player_get_stream(void *priv, char **url);
int32_t medcom_media_player_set_stream(void *priv, const char *url);
int32_t medcom_media_player_set_output_window(void *priv, uint16_t x, uint16_t y, uint16_t width, uint16_t height);

int32_t medcom_voip_login(void *priv, struct medcom_voip_account *account);
int32_t medcom_voip_logout(void *priv);
int32_t medcom_voip_connect_to(void *priv, const char *uri);
int32_t medcom_voip_accept_incoming(void *priv, char **uri);
int32_t medcom_voip_disconnect(void *priv);

#endif /* REMOTE_CONTROL_H */
