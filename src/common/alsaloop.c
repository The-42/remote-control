#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <glib.h>

#define USE_SAMPLERATE

#include "rc-alsaloop.h"

static void alsaloop_err_callback(aloop_context_t *ctx)
{
	struct alsaloop *loop = (struct alsaloop *)aloop_get_priv(ctx);

	if (loop->callback)
		loop->callback(loop->cb_data);
	else
		alsaloop_disconnect(loop);
}

static void *alsaloop_thread(void *ptr)
{
	struct alsaloop *alsaloop = (struct alsaloop *)ptr;
	aloop_context_t *ctx = alsaloop->ctx;
	aloop_t *loops[2];
	int ret = 0, i;
	char opt[32];

	snprintf(opt, sizeof(opt), "%d", alsaloop->verbose);

	aloop_set_err_callback(ctx, alsaloop_err_callback);
	/* serialize multiple open on alsa devices,
	   prevent concurrent open to fail */
	aloop_set_global(ctx, ALOOP_OPT_SERIALOPEN, NULL);
	aloop_set_global(ctx, ALOOP_OPT_VERBOSE, opt);

	for (i = 0; i < G_N_ELEMENTS(loops); i++) {
		loops[i] = aloop_loop_create(ctx);
		if (loops[i] == NULL) {
			g_warning("alsaloop: failed to create loop %d", i);
			ret = -ENOMEM;
			goto cleanup;
		}

		snprintf(opt, sizeof(opt), "%d", i);

		aloop_set_option(loops[i], ALOOP_OPT_PLAY_DEV, alsaloop->conf[i].play);
		aloop_set_option(loops[i], ALOOP_OPT_CAPT_DEV, alsaloop->conf[i].capt);
		aloop_set_option(loops[i], ALOOP_OPT_CHANNELS, alsaloop->conf[i].channels);
		/* set samplerate to: alsautils: SRC_SINC_FASTEST */
		aloop_set_option(loops[i], ALOOP_OPT_SAMPLERATE, "2");
		/* set synctype to: alsautils: sync_type_t: SYNC_TYPE_CAPTRATESHIFT */
		aloop_set_option(loops[i], ALOOP_OPT_SYNC, "4");
		aloop_set_option(loops[i], ALOOP_OPT_THREAD, opt);
	}

	/* aloop_run is blocking, return immediatly on errors but some spawned
	 * thread may be still running, "signaled" to exit but running. */
	if (aloop_run(ctx) < 0)  {
		g_warning("alsaloop: aloop_run failed");
		ret = -EIO;
	}

cleanup:
	for (i = 0; i < G_N_ELEMENTS(loops); i++) {
		if (loops[i] != NULL)
			aloop_loop_destroy(loops[i]);
	}
	return (void *)ret;
}

int alsaloop_create(struct alsaloop **alsaloopp)
{
	struct alsaloop *alsaloop;

	alsaloop = calloc(1, sizeof(struct alsaloop));
	if (alsaloop == NULL)
		return -ENOMEM;

	pthread_mutex_init(&alsaloop->ctx_mutex, NULL);

	*alsaloopp = alsaloop;
	return 0;
}

int alsaloop_connect(struct alsaloop *alsaloop)
{
	int err = 0;

	if (alsaloop == NULL)
		return -EINVAL;

	err = pthread_mutex_trylock(&alsaloop->ctx_mutex);
	if (err != 0) {
		g_warning("alsaloop: pthread_mutex_trylock failed: %s",
			  g_strerror(-err));
		return -EBUSY;
	}

	if (alsaloop->ctx != NULL) {
		g_warning("alsaloop: invalid state");
		err = -EBUSY;
		goto unlock;
	}

	if ((alsaloop->ctx = aloop_context_create()) == NULL) {
		g_warning("alsaloop: aloop_context_create failed");
		err = -ENOMEM;
		goto unlock;
	}

	aloop_set_priv(alsaloop->ctx, (void *)alsaloop);

	err = pthread_create(&alsaloop->thread, NULL,
	                     alsaloop_thread, (void *)alsaloop);
	if (err != 0) {
		g_warning("alsaloop: pthread_create failed: %s",
			  g_strerror(-err));
		aloop_context_destroy(alsaloop->ctx);
		alsaloop->ctx = NULL;
	}

unlock:
	pthread_mutex_unlock(&alsaloop->ctx_mutex);
	return err;
}

void alsaloop_disconnect(struct alsaloop *alsaloop)
{
	int err;

	if (alsaloop == NULL || alsaloop->ctx == NULL)
		return;

	err = pthread_mutex_trylock(&alsaloop->ctx_mutex);
	if (err != 0) {
		g_warning("alsaloop: pthread_mutex_trylock failed: %s",
			  g_strerror(-err));
		return;
	}

	g_debug("alsaloop: disconnecting");
	/* "signal" running threads to exit and wait */
	aloop_stop(alsaloop->ctx);

	pthread_join(alsaloop->thread, NULL);
	alsaloop->thread = 0;

	aloop_context_destroy(alsaloop->ctx);
	alsaloop->ctx = NULL;

	g_debug("alsaloop: disconnected");
	pthread_mutex_unlock(&alsaloop->ctx_mutex);
}

void alsaloop_finalize(struct alsaloop *alsaloop)
{
	if (alsaloop != NULL) {
		alsaloop_disconnect(alsaloop);

		pthread_mutex_destroy(&alsaloop->ctx_mutex);
		free(alsaloop);
		alsaloop = NULL;

		aloop_free_global();
	}
}

void alsaloop_set_error_handler(struct alsaloop* alsaloop,
				alsaloop_error_handler_cb cb,
				void *data)
{
	if (alsaloop) {
		alsaloop->callback = cb;
		alsaloop->cb_data = data;
	}
}
