#ifndef REMOTE_CONTROL_H
#define REMOTE_CONTROL_H 1

#include <stdint.h>

/**
 * media player
 */
struct media_player;

int media_player_create(struct media_player **playerp);
int media_player_free(struct media_player *player);
int media_player_set_output_window(struct media_player *player,
		unsigned int x, unsigned int y, unsigned int width,
		unsigned int height);
int media_player_set_uri(struct media_player *player, const char *uri);
int media_player_get_uri(struct media_player *player, char **urip);
int media_player_play(struct media_player *player);
int media_player_stop(struct media_player *player);

/**
 * Voice-over-IP
 */
struct voip;

int voip_create(struct voip **voipp, struct rpc_server *server);
int voip_free(struct voip *voip);
int voip_login(struct voip *voip, const char *server, uint16_t port,
		const char *username, const char *password);
int voip_logout(struct voip *voip);
int voip_call(struct voip *voip, const char *url);
int voip_accept(struct voip *voip, char **caller);
int voip_terminate(struct voip *voip);

struct remote_control {
	struct backlight *backlight;
	struct media_player *player;
	struct smartcard *smartcard;
	struct voip *voip;
};

int remote_control_create(struct remote_control **rcp);
int remote_control_free(struct remote_control *rc);

int remote_control_dispatch(struct rpc_server *server, struct rpc_packet *request);

#endif /* REMOTE_CONTROL_H */
