#ifndef REMOTE_CONTROL_H
#define REMOTE_CONTROL_H 1

#include <stdarg.h>
#include <stdint.h>

#define stringify1(x) #x
#define stringify(x)  stringify1(x)

/**
 * event manager
 */
enum event_source {
	EVENT_SOURCE_MODEM,
	EVENT_SOURCE_IO,
	EVENT_SOURCE_VOIP,
	EVENT_SOURCE_SMARTCARD,
	EVENT_SOURCE_HANDSET,
	EVENT_SOURCE_MAX,
};

struct event_modem {
};

struct event_io {
};

enum event_voip_state {
	EVENT_VOIP_STATE_OUTGOING,
	EVENT_VOIP_STATE_OUTGOING_CONNECTED,
	EVENT_VOIP_STATE_OUTGOING_DISCONNECTED,
	EVENT_VOIP_STATE_INCOMING,
	EVENT_VOIP_STATE_INCOMING_CONNECTED,
	EVENT_VOIP_STATE_INCOMING_DISCONNECTED,
};

struct event_voip {
	enum event_voip_state state;
};

enum event_smartcard_state {
	EVENT_SMARTCARD_STATE_INSERTED,
	EVENT_SMARTCARD_STATE_REMOVED,
};

struct event_smartcard {
	enum event_smartcard_state state;
};

enum event_handset_state {
	EVENT_HANDSET_STATE_HOOK_OFF,
	EVENT_HANDSET_STATE_HOOK_ON,
};

struct event_handset {
	enum event_handset_state state;
};

struct event {
	enum event_source source;

	union {
		struct event_modem modem;
		struct event_io io;
		struct event_voip voip;
		struct event_smartcard smartcard;
		struct event_handset handset;
	};
};

struct event_manager;

int event_manager_create(struct event_manager **managerp, struct rpc_server *server);
int event_manager_free(struct event_manager *manager);
int event_manager_report(struct event_manager *manager, struct event *event);
int event_manager_get_status(struct event_manager *manager, uint32_t *statusp);
int event_manager_get_source_state(struct event_manager *manager, struct event *event);

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
 * smartcard
 */
struct smartcard;

int smartcard_create(struct smartcard **smartcardp);
int smartcard_free(struct smartcard *smartcard);
int smartcard_get_type(struct smartcard *smartcard, unsigned int *typep);
ssize_t smartcard_read(struct smartcard *smartcard, off_t offset, void *buffer, size_t size);
ssize_t smartcard_write(struct smartcard *smartcard, off_t offset, const void *buffer, size_t size);

/**
 * Voice-over-IP
 */
enum voip_state {
	VOIP_STATE_LOGGED_OUT,
	VOIP_STATE_LOGGED_IN,
};

struct voip;

int voip_create(struct voip **voipp, struct rpc_server *server);
int voip_free(struct voip *voip);
int voip_login(struct voip *voip, const char *server, uint16_t port,
		const char *username, const char *password);
int voip_logout(struct voip *voip);
int voip_call(struct voip *voip, const char *url);
int voip_accept(struct voip *voip, char **caller);
int voip_terminate(struct voip *voip);
int voip_get_state(struct voip *voip, enum voip_state *statep);

/**
 * mixer
 */
struct mixer;

int mixer_create(struct mixer **mixerp);
int mixer_free(struct mixer *mixer);
int mixer_set_volume(struct mixer *mixer, unsigned short control, uint8_t volume);
int mixer_get_volume(struct mixer *mixer, unsigned short control, uint8_t *volumep);
int mixer_set_mute(struct mixer *mixer, unsigned short control, bool mute);
int mixer_is_muted(struct mixer *mixer, unsigned short control, bool *mutep);

/**
 * network layer
 */
struct net;

int net_create(struct net **netp);
int net_free(struct net *net);
ssize_t net_send_sync(struct net *net, const void *buffer, size_t size);
ssize_t net_recv_sync(struct net *net, void *buffer, size_t size);

/**
 * LLDP monitor
 */
struct lldp_monitor;

int lldp_monitor_create(struct lldp_monitor **monitorp);
int lldp_monitor_free(struct lldp_monitor *monitor);
ssize_t lldp_monitor_read(struct lldp_monitor *monitor, void *buffer,
		size_t size);

/**
 * remote control
 */
struct remote_control {
	struct event_manager *event_manager;
	struct backlight *backlight;
	struct media_player *player;
	struct smartcard *smartcard;
	struct voip *voip;
	struct mixer *mixer;
	struct net *net;
	struct lldp_monitor *lldp;
};

int remote_control_create(struct remote_control **rcp);
int remote_control_free(struct remote_control *rc);

int remote_control_dispatch(struct rpc_server *server, struct rpc_packet *request);

/**
 * utilities
 */
#define RC_EMERG   "<0>" /* system is unusable */
#define RC_ALERT   "<1>" /* action must be taken immediately */
#define RC_CRIT    "<2>" /* critical conditions */
#define RC_ERR     "<3>" /* error conditions */
#define RC_WARNING "<4>" /* warning conditions */
#define RC_NOTICE  "<5>" /* normal but significant condition */
#define RC_INFO    "<6>" /* informational */
#define RC_DEBUG   "<7>" /* debug-level messages */

#define RC_CONT    ""

void rc_logv(const char *fmt, va_list ap);
void rc_log(const char *fmt, ...);

enum {
	DUMP_PREFIX_NONE,
	DUMP_PREFIX_ADDRESS,
	DUMP_PREFIX_OFFSET,
};

void print_hex_dump(const char *level, const char *prefix_str, int prefix_type,
		size_t rowsize, const void *buffer, size_t size, bool ascii);

#endif /* REMOTE_CONTROL_H */
