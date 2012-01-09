#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <getopt.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>

#include <glib.h>

#include "remote-control-irkey.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a))/(sizeof((a)[0]))
#endif

#define MAX_QUEUE_DEPTH 10
#define DEFAULT_IR_PORT "/dev/ttyHS2"

struct irkey {
	int tty;
	GQueue *fifo;
#if GLIB_CHECK_VERSION(2, 31, 0)
	GMutex lock;
#else
	GMutex *lock;
#endif
	GThread *thread;
	gboolean running;
};

static ssize_t read_all(int fd, void *buffer, size_t count, unsigned long timeout)
{
	struct timeval to;
	ssize_t pos = 0;
	ssize_t num;

	memset(&to, 0, sizeof(to));
	to.tv_sec = timeout / 1000;
	to.tv_usec = (timeout % 1000) * 1000;

	while (pos < count) {
		fd_set rfds;
		int err;

		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);

		err = select(fd + 1, &rfds, NULL, NULL, &to);
		if (err < 0) {
			if (errno == EINTR)
				continue;

			return -errno;
		}

		if (err == 0)
			return -ETIMEDOUT;

		if (FD_ISSET(fd, &rfds)) {
			num = read(fd, buffer + pos, count - pos);
			if (num < 0) {
				if (errno == EINTR)
					continue;

				return -errno;
			}

			pos += num;
		}
	}

	return pos;
}

static void flush_queue(GQueue *queue)
{
	gpointer item;
	while (1) {
		item = g_queue_pop_head(queue);
		if (!item)
			break;
		g_free(item);
	}
}

static void irk_lock_queue(struct irkey *ctx)
{
#if GLIB_CHECK_VERSION(2, 31, 0)
	g_mutex_lock(&ctx->lock);
#else
	g_mutex_lock(ctx->lock);
#endif
}

static void irk_unlock_queue(struct irkey *ctx)
{
#if GLIB_CHECK_VERSION(2, 31, 0)
	g_mutex_unlock(&ctx->lock);
#else
	g_mutex_unlock(ctx->lock);
#endif
}

/**
 * Open serial port and configure the port
 */
static int irk_open_tty(struct irkey *ctx, const char *tty)
{
	struct termios attr;
	int ret;

	int fd = open(tty, O_RDWR | O_NOCTTY, 0);
	if (fd < 0) {
		g_warning("%s: failed to open device: [%s]", __func__, tty);
		return -ENOENT;
	}

	ret = tcgetattr(fd, &attr);
	if (ret != 0)
		g_warning("%s: fail to get tty settings", __func__);

	attr.c_cflag = B9600 | CS8 | CLOCAL | CREAD;
	attr.c_iflag = IGNPAR | BRKINT;
	attr.c_oflag = 0;
	attr.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

	ret = tcsetattr(fd, TCSANOW, &attr);
	if (ret != 0)
		g_warning("%s: failed to set tty settings", __func__);

	tcflush(fd, TCIFLUSH);

	return (ctx->tty = fd);
}

void irk_free(struct irkey *ctx)
{
	if (ctx == NULL)
		return;

	if (ctx->thread && ctx->running)
		irk_close_thread(ctx);

	if (ctx->fifo) {
		flush_queue(ctx->fifo);
		g_queue_free(ctx->fifo);
		ctx->fifo = NULL;
	}
#if !GLIB_CHECK_VERSION(2, 31, 0)
	if (ctx->lock) {
		g_mutex_free(ctx->lock);
		ctx->lock = NULL;
	}
#endif
	if (ctx->tty) {
		close(ctx->tty);
		ctx->tty = 0;
	}

	g_free(ctx);
	ctx = NULL;
}

struct irkey* irk_new()
{
	struct irkey* irk;

	irk = g_new(struct irkey, 1);
	if (!irk)
		return NULL;

	memset(irk, 0, sizeof(*irk));

	irk->fifo = g_queue_new();
	if (!irk->fifo) {
		g_free(irk);
		return NULL;
	}
#if GLIB_CHECK_VERSION(2, 31, 0)
	g_mutex_init(&irk->lock);
#else
	irk->lock = g_mutex_new();
	if (!irk->lock) {
		g_queue_free(irk->fifo);
		g_free(irk);
		return NULL;
	}
#endif
	return irk;
}

struct irkey* irk_new_with_tty(const gchar *tty)
{
	struct irkey* irk;
	int err;

	irk = irk_new();
	if (!irk)
		return NULL;

	err = irk_open_tty(irk, tty);
	if (err < 0) {
		irk_free(irk);
		irk = NULL;
	}

	return irk;
}

int irk_peek_message(struct irkey *ctx, struct ir_message **msg)
{
	struct ir_message *item;

	if (!ctx || !msg)
		return -EINVAL;

	irk_lock_queue(ctx);
	item = g_queue_pop_head(ctx->fifo);
	irk_unlock_queue(ctx);

	if (!item)
		return -ENODATA;

	/* TODO: it might not be a good idea to hand over the ownership
	 *       to the user.
	memcpy(item, *msg, sizeof(*item));
	g_free(item); */

	*msg = item;
	return 0;
}

static int irk_enqueue_message(struct irkey *ctx, struct ir_message *msg)
{
	struct ir_message *item;

	if (!ctx || !msg)
		return -EINVAL;

	irk_lock_queue(ctx);

	/* drop the oldes event if max depth has been reached. */
	if (g_queue_get_length(ctx->fifo) > MAX_QUEUE_DEPTH)
		item = g_queue_pop_head(ctx->fifo);
	else
		item = g_new(struct ir_message, 1);

	if (!item) {
		irk_unlock_queue(ctx);
		return -ENOMEM;
	}

	memcpy(item, msg, sizeof(*msg));
	g_queue_push_tail(ctx->fifo, item);
	irk_unlock_queue(ctx);

//	g_debug("   added: %02x %02x %02x %02x %02x %02x %02x %02x",
//		item->header, item->reserved, item->d0, item->d1, item->d2,
//		item->d3, item->d4, item->d5);
	return 0;
}

/**
 * Process the buffer we have received.
 */
static int irk_process_buffer(struct irkey *ctx, uint8_t *buf, size_t len)
{
	struct ir_message *msg;
	uint8_t proto;
	int err;

	if (len != sizeof(struct ir_message)) {
		g_warning("%s: invalid length", __func__);
		return -ENOPROTOOPT;
	}

	msg = (struct ir_message*)buf;
	proto = (msg->header & HEADER_PROTOCOL_MASK) >> HEADER_PROTOCOL_SHIFT;

	switch (proto) {
	case HEADER_PROTOCOL_RC5:
	case HEADER_PROTOCOL_LG:
		err = irk_enqueue_message(ctx, msg);
		break;

	default:
		g_warning("%s: unsupported protocol: %.2x",
			__func__, msg->header);
		err = -EPROTONOSUPPORT;
		break;
	}

	return err;
}

static gpointer irk_thread_proc(gpointer data)
{
	struct irkey *ctx = data;
	uint8_t buf[8]; /* messages are normaly 64bits */
	ssize_t got;
	fd_set fds;
	int ret;

	/* we can only work with a opend serial port */
	if (!ctx->tty) {
		ret = irk_open_tty(ctx, DEFAULT_IR_PORT);
		if (ret < 0)
			goto cleanup;
	}

	FD_ZERO(&fds);
	FD_SET(ctx->tty, &fds);

	while (ctx->running) {
		memset(buf, 0, sizeof(buf));

		ret = select(FD_SETSIZE, &fds, NULL, NULL, NULL);
		if (!ret) {
			g_warning("%s:select failed %d", __func__, ret);
			/* what now? */
			continue;
		}

		got = read_all(ctx->tty, buf, sizeof(buf), 100);
		if (got < 0) {
			if (got == -ETIMEDOUT)
				continue;
			g_warning("%s: read failed: %zd", __func__, got);
			break;
		}

		ret = irk_process_buffer(ctx, buf, got);
		if (ret < 0) {
			g_warning("%s: processing buffer failed: %d",
				__func__, ret);
		}
	}

	ctx->running = FALSE;

cleanup:
	return NULL;
}

int irk_setup_thread(struct irkey *ctx)
{
#if GLIB_CHECK_VERSION(2, 31, 0)
	ctx->running = TRUE;
	ctx->thread = g_thread_new("irk_thread", irk_thread_proc, ctx);
	if (!ctx->thread) {
		g_warning("%s: failed to create thread", __func__);
	}
#else
	GError *error = NULL;

	ctx->running = TRUE;
	ctx->thread = g_thread_create(irk_thread_proc, ctx, TRUE, &error);
	if (!ctx->thread) {
		g_warning("%s: failed to create thread: %s",
			__func__, error->message);
		g_error_free(error);
	}
#endif
	return 0;
}

int irk_close_thread(struct irkey *ctx)
{
	ctx->running = FALSE;
	g_thread_join(ctx->thread);
	ctx->thread = NULL;

	return 0;
}