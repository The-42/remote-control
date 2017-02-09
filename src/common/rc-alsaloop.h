/*
 * Copyright (C) 2010-2012 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef REMOTE_CONTROL_ALSALOOP_H
#define REMOTE_CONTROL_ALSALOOP_H 1

#include <alsaloop.h>
#include <pthread.h>

#ifndef ALSALOOP_LOOPS
#define ALSALOOP_LOOPS 2
#endif

typedef void (*alsaloop_error_handler_cb)(void *data);

struct alsaloop_conf {
	char *play;
	char *capt;
	char *channels;
};

struct alsaloop {
	aloop_context_t *ctx;

	pthread_mutex_t ctx_mutex;
	pthread_t thread;

	struct alsaloop_conf conf[ALSALOOP_LOOPS];

	alsaloop_error_handler_cb callback;
	void *cb_data;

	int valid_conf;
	int verbose;
	int quit;
	int iret;
};

int alsaloop_create(struct alsaloop **alsaloop);
int alsaloop_connect(struct alsaloop *alsaloop);
void alsaloop_disconnect(struct alsaloop *alsaloop);
void alsaloop_finalize(struct alsaloop *alsaloop);

void alsaloop_set_error_handler(struct alsaloop* alsaloop,
				alsaloop_error_handler_cb cb,
				void *data);

#endif
