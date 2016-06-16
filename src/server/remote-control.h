/*
 * Copyright (C) 2010-2012 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef REMOTE_CONTROL_H
#define REMOTE_CONTROL_H 1

#include <stdint.h>

#include <gtk/gtk.h>
#include <glib.h>

#include <librpc.h>

#define BIT(x) (1 << (x))

/**
 * event manager
 */
enum event_source {
	EVENT_SOURCE_MODEM,
	EVENT_SOURCE_IO,
	EVENT_SOURCE_VOIP,
	EVENT_SOURCE_SMARTCARD,
	EVENT_SOURCE_HOOK,
	EVENT_SOURCE_HANDSET,
	EVENT_SOURCE_MAX,
};

enum event_modem_state {
	EVENT_MODEM_STATE_RINGING,
	EVENT_MODEM_STATE_CONNECTED,
	EVENT_MODEM_STATE_DISCONNECTED,
	EVENT_MODEM_STATE_ERROR,
};

struct event_modem {
	enum event_modem_state state;
};

struct event_io {
};

enum event_voip_state {
	EVENT_VOIP_STATE_IDLE,
	EVENT_VOIP_STATE_LOGGED_ON,
	EVENT_VOIP_STATE_LOGGED_OFF,
	EVENT_VOIP_STATE_OUTGOING,
	EVENT_VOIP_STATE_OUTGOING_CONNECTED,
	EVENT_VOIP_STATE_OUTGOING_DISCONNECTED,
	EVENT_VOIP_STATE_INCOMING,
	EVENT_VOIP_STATE_INCOMING_CONNECTED,
	EVENT_VOIP_STATE_INCOMING_DISCONNECTED,
	EVENT_VOIP_STATE_INCOMING_MISSED,
	EVENT_VOIP_STATE_OUTGOING_EARLYMEDIA,
	EVENT_VOIP_STATE_INCOMING_EARLYMEDIA,
	EVENT_VOIP_STATE_ERROR_USER_BUSY,
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

enum event_hook_state {
	EVENT_HOOK_STATE_OFF,
	EVENT_HOOK_STATE_ON,
};

struct event_hook {
	enum event_hook_state state;
};

struct event_handset {
	unsigned int keycode;
	bool pressed;
};

struct event {
	enum event_source source;

	union {
		struct event_modem modem;
		struct event_io io;
		struct event_voip voip;
		struct event_smartcard smartcard;
		struct event_hook hook;
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
 * audio state
 */
enum audio_state {
	AUDIO_STATE_INACTIVE,
	AUDIO_STATE_HIFI_PLAYBACK_SPEAKER,
	AUDIO_STATE_HIFI_PLAYBACK_HEADSET,
	AUDIO_STATE_VOICECALL_HANDSET,
	AUDIO_STATE_VOICECALL_HEADSET,
	AUDIO_STATE_VOICECALL_SPEAKER,
	AUDIO_STATE_VOICECALL_IP_HANDSET,
	AUDIO_STATE_VOICECALL_IP_HEADSET,
	AUDIO_STATE_VOICECALL_IP_SPEAKER,
	AUDIO_STATE_LINEIN_SPEAKER,
	AUDIO_STATE_LINEIN_HEADSET
};

struct audio;

int audio_create(struct audio **audiop, struct rpc_server *server, GKeyFile *config);
int audio_free(struct audio *audio);
int audio_set_state(struct audio *audio, enum audio_state state);
int audio_get_state(struct audio *audio, enum audio_state *statep);
int audio_set_volume(struct audio *audio, uint8_t volume);
int audio_get_volume(struct audio *audio, uint8_t *volumep);
int audio_set_speakers_enable(struct audio *audio, bool enable);
int audio_get_speakers_enable(struct audio *audio, bool *enablep);

/**
 * backlight
 */
#define BACKLIGHT_MIN 0x00
#define BACKLIGHT_MAX 0xff

struct backlight;

int backlight_create(struct backlight **backlightp);
int backlight_free(struct backlight *backlight);
int backlight_enable(struct backlight *backlight, bool enable);
int backlight_is_enabled(struct backlight *backlight);
int backlight_set(struct backlight *backlight, unsigned int brightness);
int backlight_get(struct backlight *backlight);

/**
 * cursor movement
 */
struct cursor_movement;

int cursor_movement_create(struct cursor_movement **cursor_movement);
int cursor_movement_free(struct cursor_movement *cursor_movement);
int cursor_movement_set_timeout(struct cursor_movement *cursor_movement, int timeout);
int cursor_movement_get_timeout(struct cursor_movement *cursor_movement);

/**
 * media player
 */
enum media_player_state {
	MEDIA_PLAYER_STOPPED,
	MEDIA_PLAYER_PLAYING,
	MEDIA_PLAYER_PAUSED,
	MEDIA_PLAYER_STARTING,
	MEDIA_PLAYER_STOPPING,
};

enum media_player_es_action {
	MEDIA_PLAYER_ES_ADDED,
	MEDIA_PLAYER_ES_DELETED,
	MEDIA_PLAYER_ES_SELECTED,
};

enum media_player_es_type {
	MEDIA_PLAYER_ES_UNKNOWN,
	MEDIA_PLAYER_ES_AUDIO,
	MEDIA_PLAYER_ES_VIDEO,
	MEDIA_PLAYER_ES_TEXT,
};

struct media_player;

typedef void (* media_player_es_changed_cb)( void *data,
		enum media_player_es_action action,
		enum media_player_es_type type, int pid);

int media_player_create(struct media_player **playerp, GKeyFile *config);
int media_player_free(struct media_player *player);
int media_player_set_crop(struct media_player *player,
		unsigned int left, unsigned int right, unsigned int top,
		unsigned int bottom);
int media_player_set_output_window(struct media_player *player,
		unsigned int x, unsigned int y, unsigned int width,
		unsigned int height);
int media_player_set_uri(struct media_player *player, const char *uri);
int media_player_get_uri(struct media_player *player, char **urip);
int media_player_play(struct media_player *player);
int media_player_stop(struct media_player *player);
int media_player_pause(struct media_player *player);
int media_player_resume(struct media_player *player);
int media_player_get_duration(struct media_player *player,
		unsigned long *duration);
int media_player_get_position(struct media_player *player,
		unsigned long *position);
int media_player_set_position(struct media_player *player,
		unsigned long position);
int media_player_get_state(struct media_player *player,
		enum media_player_state *statep);
int media_player_get_mute(struct media_player *player, bool *mute);
int media_player_set_mute(struct media_player *player, bool mute);
int media_player_get_audio_track_count(struct media_player *player, int *count);
int media_player_get_audio_track_pid(struct media_player *player, int pos, int *pid);
/* Return value name will be allocated! */
int media_player_get_audio_track_name(struct media_player *player, int pid, char **name);
int media_player_get_audio_track(struct media_player *player, int *pid);
int media_player_set_audio_track(struct media_player *player, int pid);
int media_player_get_spu_count(struct media_player *player, int *count);
int media_player_get_spu_pid(struct media_player *player, int pos, int *pid);
/* Return value name will be allocated! */
int media_player_get_spu_name(struct media_player *player, int pid, char **name);
int media_player_get_spu(struct media_player *player, int *pid);
int media_player_set_spu(struct media_player *player, int pid);
int media_player_get_teletext(struct media_player *player, int *page);
int media_player_set_teletext(struct media_player *player, int page);
int media_player_toggle_teletext_transparent(struct media_player *player);
int media_player_set_es_changed_callback(struct media_player *player,
		media_player_es_changed_cb callback, void *data);

/**
 * sound manager
 */
enum sound_manager_state {
	SOUND_MANAGER_STOPPED,
	SOUND_MANAGER_PLAYING,
	SOUND_MANAGER_PAUSED,
};

struct sound_manager;

int sound_manager_create(struct sound_manager **managerp, struct audio *audio,
		GKeyFile *config);
int sound_manager_free(struct sound_manager *manager);
int sound_manager_play(struct sound_manager *manager, const char *uri);
int sound_manager_pause(struct sound_manager *manager);
int sound_manager_stop(struct sound_manager *manager);
int sound_manager_get_state(struct sound_manager *manager,
		enum sound_manager_state *statep);

/**
 * smartcard
 */
enum smartcard_type {
	SMARTCARD_TYPE_UNKNOWN,
	SMARTCARD_TYPE_I2C,
	SMARTCARD_TYPE_T0,
	SMARTCARD_TYPE_T1,
};

struct smartcard;

int smartcard_create(struct smartcard **smartcardp, struct rpc_server *server,
		     GKeyFile *config);
int smartcard_free(struct smartcard *smartcard);
int smartcard_get_type(struct smartcard *smartcard, enum smartcard_type *typep);
ssize_t smartcard_read(struct smartcard *smartcard, off_t offset, void *buffer, size_t size);
ssize_t smartcard_write(struct smartcard *smartcard, off_t offset, const void *buffer, size_t size);

struct smartcard_info {
	char *first_name;
	char *last_name;
	char *date_of_birth;
	char *gender;
	char *zip_code;
	char *city;
	char *country;
	char *street;
	char *street_number;
	char *insurance_id;
	char *insurer_id;
	char *insurer_country;
	char *insurer_name;
	char *billing_insurer_id;
	char *billing_insurer_name;
	char *card_id;
	char *card_atr;
	char *atr;
};
ssize_t smartcard_read_info(struct smartcard *smartcard, struct smartcard_info *data);
void smartcard_read_info_free(struct smartcard_info *data);

/**
 * modem manager
 */
enum modem_state {
	MODEM_STATE_IDLE,
	MODEM_STATE_RINGING,
	MODEM_STATE_INCOMING,
	MODEM_STATE_OUTGOING,
	MODEM_STATE_ACTIVE,
};

struct modem_manager;

int modem_manager_create(struct modem_manager **managerp,
		struct rpc_server *server, GKeyFile *config);
GSource *modem_manager_get_source(struct modem_manager *manager);
int modem_manager_initialize(struct modem_manager *manager);
int modem_manager_shutdown(struct modem_manager *manager);
int modem_manager_call(struct modem_manager *manager, const char *number);
int modem_manager_accept(struct modem_manager *manager);
int modem_manager_terminate(struct modem_manager *manager);
int modem_manager_get_state(struct modem_manager *manager, enum modem_state *statep);

/**
 * Voice-over-IP
 */
enum voip_transport {
	VOIP_TRANSPORT_UDP,
	VOIP_TRANSPORT_TCP,
	VOIP_TRANSPORT_TLS,
	VOIP_TRANSPORT_MAX,
};

enum voip_state {
	VOIP_STATE_LOGGED_OUT,
	VOIP_STATE_LOGGED_IN,
};

struct voip;

int voip_create(struct voip **voipp, struct rpc_server *server,
		GKeyFile *config);
int voip_free(struct voip *voip);
GSource *voip_get_source(struct voip *voip);
int voip_login(struct voip *voip, const char *server, uint16_t port,
	       const char *username, const char *password,
	       enum voip_transport transport);
int voip_logout(struct voip *voip);
int voip_call(struct voip *voip, const char *url);
int voip_accept(struct voip *voip, char **caller);
int voip_terminate(struct voip *voip);
int voip_get_state(struct voip *voip, enum voip_state *statep);
int voip_get_contact(struct voip *voip, const char **namep, const char **displayp);
int voip_dial(struct voip *voip, uint8_t dtmf);
int voip_set_playback(struct voip *voip, const char *card_name);
int voip_set_capture(struct voip *voip, const char *card_name);
int voip_set_capture_gain(struct voip *voip, float gain);

/**
 * mixer
 */
enum mixer_control {
	MIXER_CONTROL_UNKNOWN,
	MIXER_CONTROL_PLAYBACK_MASTER,
	MIXER_CONTROL_PLAYBACK_PCM,
	MIXER_CONTROL_PLAYBACK_HEADSET,
	MIXER_CONTROL_PLAYBACK_SPEAKER,
	MIXER_CONTROL_PLAYBACK_HANDSET,
	MIXER_CONTROL_CAPTURE_MASTER,
	MIXER_CONTROL_MAX,
};

enum mixer_input_source {
	MIXER_INPUT_SOURCE_UNKNOWN,
	MIXER_INPUT_SOURCE_HEADSET,
	MIXER_INPUT_SOURCE_HANDSET,
	MIXER_INPUT_SOURCE_LINE,
	MIXER_INPUT_SOURCE_MAX,
};

struct mixer;

int mixer_create(struct mixer **mixerp);
GSource *mixer_get_source(struct mixer *mixer);
int mixer_set_volume(struct mixer *mixer, unsigned short control, unsigned int volume);
int mixer_get_volume(struct mixer *mixer, unsigned short control, unsigned int *volumep);
int mixer_set_mute(struct mixer *mixer, unsigned short control, bool mute);
int mixer_is_muted(struct mixer *mixer, unsigned short control, bool *mutep);
int mixer_set_input_source(struct mixer *mixer, enum mixer_input_source source);
int mixer_get_input_source(struct mixer *mixer, enum mixer_input_source *sourcep);
int mixer_loopback_enable(struct mixer *mixer, bool enable);
int mixer_loopback_is_enabled(struct mixer *mixer, bool *enabled);

/**
 * network layer
 */
struct net;

int net_create(struct net **netp, struct rpc_server *server);
int net_free(struct net *net);
int net_configure(struct net *net, const char *hostname, uint16_t port,
		unsigned long timeout, unsigned int repeat);
ssize_t net_send_async(struct net *net, const void *buffer, size_t size);
ssize_t net_send_sync(struct net *net, const void *buffer, size_t size);
ssize_t net_recv_async(struct net *net, void *buffer, size_t size);
ssize_t net_recv_sync(struct net *net, void *buffer, size_t size);

/**
 * LLDP monitor
 */
#define LLDP_MAX_SIZE 1536

struct lldp_monitor;

int lldp_monitor_create(struct lldp_monitor **monitorp, GKeyFile *config);
GSource *lldp_monitor_get_source(struct lldp_monitor *monitor);
ssize_t lldp_monitor_read(struct lldp_monitor *monitor, void *buffer,
		size_t size);

/**
 * task manager
 */
typedef void(*task_terminate_cb)(int, void*);
struct task_manager;

int task_manager_create(struct task_manager **managerp);
int task_manager_free(struct task_manager *manager);
int32_t task_manager_exec(void *priv, const char *command_line,
		task_terminate_cb terminate_cb, void *callback_data);
int32_t task_manager_kill(void *priv, int32_t pid, int32_t sig);

/**
 * tuner
 */
struct tuner;

int tuner_create(struct tuner **tunerp);
int tuner_free(struct tuner *tuner);
int tuner_set_frequency(struct tuner *tuner, unsigned long frequency);
int tuner_set_input(struct tuner *tuner, int input_nr);
int tuner_set_standard(struct tuner *tuner, const char *standard);

/**
 * handset
 */
struct handset;

int handset_create(struct handset **handsetp, struct rpc_server *server);
int handset_free(struct handset *handset);

int handset_display_clear(struct handset *handset);
int handset_display_sync(struct handset *handset);
int handset_display_set_brightness(struct handset *handset,
		unsigned int brightness);
int handset_keypad_set_brightness(struct handset *handset,
		unsigned int brightness);
int handset_icon_show(struct handset *handset, unsigned int id, bool show);
int handset_text_show(struct handset *handset, unsigned int x, unsigned int y,
		const char *text, bool show);

/**
 * GPIO backend and event source
 */
enum gpio {
	GPIO_UNKNOWN,
	GPIO_HANDSET,
	GPIO_SMARTCARD,
	GPIO_NUM
};

struct gpio_backend;

int gpio_backend_create(struct gpio_backend **backendp, struct event_manager *events,
		GKeyFile *config);
int gpio_backend_free(struct gpio_backend *backend);
GSource *gpio_backend_get_source(struct gpio_backend *backend);
int gpio_backend_get_num_gpios(struct gpio_backend *backend);
int gpio_backend_direction_input(struct gpio_backend *backend, unsigned int gpio);
int gpio_backend_direction_output(struct gpio_backend *backend, unsigned int gpio,
		int value);
int gpio_backend_set_value(struct gpio_backend *backend, unsigned int gpio, int value);
int gpio_backend_get_value(struct gpio_backend *backend, unsigned int gpio);

/**
 * Application Watchdog
 */
struct app_watchdog;

int app_watchdog_create(struct app_watchdog **watchdogp, GKeyFile *config);
int app_watchdog_free(struct app_watchdog *watchdog);
int app_watchdog_start(struct app_watchdog *watchdog, int interval);
int app_watchdog_stop(struct app_watchdog *watchdog);
int app_watchdog_trigger(struct app_watchdog *watchdog);

/**
 * remote control
 */
struct remote_control;

int remote_control_create(struct remote_control **rcp, GKeyFile *config);
GSource *remote_control_get_source(struct remote_control *rc);
int remote_control_free(struct remote_control *rc);

struct event_manager *remote_control_get_event_manager(struct remote_control *rc);
struct audio* remote_control_get_audio(struct remote_control *rc);
struct backlight *remote_control_get_backlight(struct remote_control *rc);
struct cursor_movement *remote_control_get_cursor_movement(struct remote_control *rc);
struct media_player *remote_control_get_media_player(struct remote_control *rc);
struct sound_manager *remote_control_get_sound_manager(struct remote_control *rc);
struct smartcard *remote_control_get_smartcard(struct remote_control *rc);
struct modem_manager *remote_control_get_modem_manager(struct remote_control *rc);
struct voip *remote_control_get_voip(struct remote_control *rc);
struct mixer *remote_control_get_mixer(struct remote_control *rc);
struct net *remote_control_get_net(struct remote_control *rc);
struct lldp_monitor *remote_control_get_lldp_monitor(struct remote_control *rc);
struct task_manager *remote_control_get_task_manager(struct remote_control *rc);
struct tuner *remote_control_get_tuner(struct remote_control *rc);
struct handset *remote_control_get_handset(struct remote_control *rc);
struct gpio_backend *remote_control_get_gpio_backend(struct remote_control *rc);
struct app_watchdog *remote_control_get_watchdog(struct remote_control *rc);

int remote_control_dispatch(struct rpc_server *server, struct rpc_packet *request);

/**
 * USB Handset
 */
int usb_handset_create(struct remote_control *rc);

/**
 * utilities
 */
unsigned int if_lookup_default(void);

#if GTK_CHECK_VERSION(2, 91, 0)
void gdk_window_clear(GdkWindow *window);
#endif

#endif /* REMOTE_CONTROL_H */
