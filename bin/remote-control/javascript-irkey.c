/*
 * Copyright (C) 2012 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>

#include "javascript.h"

/*
 * TODO: Make this a configuration setting.
 */
#define DEFAULT_IR_PORT_VIBRANTE "/dev/ttyHS2"
#define DEFAULT_IR_PORT_CHROMIUM "/dev/ttyS2" /* S1 is modem */

#define HEADER_VERSION_MASK     0x0F
#define HEADER_VERSION_SHIFT    0
#define HEADER_PROTOCOL_MASK    0xF0
#define HEADER_PROTOCOL_SHIFT   4
#define HEADER_PROTOCOL_RC5     0x1
#define HEADER_PROTOCOL_RC6     0x2
#define HEADER_PROTOCOL_LG      0x3

struct ir_message {
	uint8_t header;
	uint8_t reserved;
	union {
		struct {
			uint8_t d5;
			uint8_t d4;
			uint8_t d3;
			uint8_t d2;
			uint8_t d1;
			uint8_t d0;
		};
		uint8_t data[6];
	};
};

struct ir {
	GSource source;
	GPollFD *tty;

	JSContextRef context;
	JSObjectRef callback;
	JSObjectRef thisptr;
};

static ssize_t read_all(int fd, void *buffer, size_t count, ulong timeout)
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

static void print_exception(JSContextRef context, JSValueRef exception)
{
	JSStringRef text;
	size_t length;
	char *buf;

	if (!JSValueIsString(context, exception))
		return;

	text = JSValueToStringCopy(context, exception, NULL);
	if (!text)
		return;

	length = JSStringGetMaximumUTF8CStringSize(text);
	buf = g_malloc0(length+1);
	JSStringGetUTF8CString(text, buf, length);

	g_warning("%s", buf);

	JSStringRelease(text);
	g_free(buf);
}

static int ir_report(struct ir *ir, struct ir_message *message)
{
	JSValueRef exception = NULL;
	JSValueRef arguments[1];
	JSValueRef array[8];
	int ret = 0;

	/* ir object has been used but callback has not been set */
	if (ir->callback == NULL)
		return 0;

	/*
	 * TODO: check if this will open up a memory leaks here,
	 *       or if GC takes care
	 */
	array[0] = JSValueMakeNumber(ir->context, message->header);
	array[1] = JSValueMakeNumber(ir->context, message->reserved);
	array[2] = JSValueMakeNumber(ir->context, message->d0);
	array[3] = JSValueMakeNumber(ir->context, message->d1);
	array[4] = JSValueMakeNumber(ir->context, message->d2);
	array[5] = JSValueMakeNumber(ir->context, message->d3);
	array[6] = JSValueMakeNumber(ir->context, message->d4);
	array[7] = JSValueMakeNumber(ir->context, message->d5);

	arguments[0] = JSObjectMakeArray(ir->context, G_N_ELEMENTS(array),
	                                 array, &exception);

	(void)JSObjectCallAsFunction(ir->context, ir->callback,
			ir->thisptr, G_N_ELEMENTS(arguments), arguments,
			&exception);
	if (exception) {
		g_warning("js-irkey: exception in callback");
		print_exception(ir->context, exception);
		ret = -EFAULT;
	}

	return ret;
}

static gboolean ir_source_prepare(GSource *source, gint *timeout)
{
	if (timeout)
		*timeout = -1;

	return FALSE;
}

static gboolean ir_source_check(GSource *source)
{
	struct ir *ir = (struct ir *)source;
	GPollFD *poll = ir->tty;

	if (poll->revents & G_IO_IN)
		return TRUE;

	return FALSE;
}

static gboolean ir_source_dispatch(GSource *source, GSourceFunc callback,
		gpointer user_data)
{
	struct ir *ir = (struct ir *)source;
	struct ir_message *msg = NULL;
	GPollFD *poll = ir->tty;
	uint8_t buf[8]; /* messages are normaly 64bits */
	uint8_t proto;
	ssize_t got;
	int err;

	got = read_all(poll->fd, buf, sizeof(buf), 100);
	if (got < 0) {
		g_warning("js-irkey: read failed: %zd", got);
		goto fail;
	}

	if (got != sizeof(struct ir_message)) {
		g_warning("js-irkey: invalid length");
		goto fail;
	}

	msg = (struct ir_message*)buf;
	proto = (msg->header & HEADER_PROTOCOL_MASK) >> HEADER_PROTOCOL_SHIFT;

	switch (proto) {
	case HEADER_PROTOCOL_RC5:
	case HEADER_PROTOCOL_LG:
		err = ir_report(ir, msg);
		if (err < 0) {
			g_warning("js-irkey: ir_report() failed: %s",
				  g_strerror(-err));
			goto fail;
		}
		break;

	default:
		g_warning("js-irkey: unsupported protocol: %.2x",
			  msg->header);
		break;
	}

fail:
	if (callback)
		return callback(user_data);

	return TRUE;
}

static void ir_source_finalize(GSource *source)
{
	struct ir *ir = (struct ir *)source;

	if (ir->tty) {
		close(ir->tty->fd);
		ir->tty->fd = 0;
		g_free(ir->tty);
		ir->tty = NULL;
	}
}

static GSourceFuncs ir_source_funcs = {
	.prepare = ir_source_prepare,
	.check = ir_source_check,
	.dispatch = ir_source_dispatch,
	.finalize = ir_source_finalize,
};

static int open_tty(const char *tty, int *fd)
{
	struct termios attr;
	int ret;

	ret = open(tty, O_RDWR | O_NOCTTY, 0);
	if (ret == -1) {
		g_debug("js-irkey: failed to open device: [%s]", tty);
		return -errno;
	}

	*fd = ret;

	ret = tcgetattr(*fd, &attr);
	if (ret != 0) {
		g_warning("js-irkey: failed to get TTY settings on %s: %s",
			  tty, g_strerror(errno));
		return -errno;
	}

	attr.c_cflag = B9600 | CS8 | CLOCAL | CREAD;
	attr.c_iflag = IGNPAR | BRKINT;
	attr.c_oflag = 0;
	attr.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

	ret = tcsetattr(*fd, TCSANOW, &attr);
	if (ret < 0) {
		g_warning("js-irkey: failed to set TTY settings on %s: %s",
			  tty, g_strerror(errno));
		return -errno;
	}

	ret = tcflush(*fd, TCIFLUSH);
	if (ret < 0) {
		g_warning("js-irkey: failed to flush TTY on %s: %s",
			  tty, g_strerror(errno));
	}

	return 0;
}

static int ir_setup_poll(struct ir *ir, int fd)
{
	g_return_val_if_fail(ir != NULL, -EINVAL);
	g_return_val_if_fail(fd != 0, -EINVAL);

	ir->tty = g_new0(GPollFD, 1);
	if (!ir->tty) {
		close(fd);
		return -ENOMEM;
	}

	ir->tty->events = G_IO_IN | G_IO_HUP | G_IO_ERR;
	ir->tty->fd = fd;

	g_source_add_poll((GSource *)ir, ir->tty);
	return 0;
}

static GSource *ir_source_new(JSContextRef context)
{
	static const char* ttys[] = {
		DEFAULT_IR_PORT_VIBRANTE,
		DEFAULT_IR_PORT_CHROMIUM
	};

	GSource *source;
	struct ir *ir;
	int fd = 0;
	int ret;
	int i;

	source = g_source_new(&ir_source_funcs, sizeof(*ir));
	if (!source) {
		g_warning("js-irkey: failed to allocate memory");
		return NULL;
	}

	ir = (struct ir *)source;
	ir->context = context;
	ir->callback = NULL;

	for (i=0; i<G_N_ELEMENTS(ttys); i++) {
		ret = open_tty(ttys[i], &fd);
		if (ret < 0) {
			if (ret == -ENOENT)
				continue;
			g_warning("js-irkey: failed to open TTY: %s",
				  g_strerror(-ret));
			goto cleanup;
		}
		g_debug("js-irkey: using port [%s]", ttys[i]);
		break;
	}

	ret = ir_setup_poll(ir, fd);
	if (ret < 0) {
		g_warning("js-irkey: failed to create polling fd: %s",
			  g_strerror(-ret));
		goto cleanup;
	}

	return source;

cleanup:
	g_source_unref(source);
	return NULL;
}

static void ir_initialize(JSContextRef context, JSObjectRef object)
{
	struct ir *ir = JSObjectGetPrivate(object);

	ir->thisptr = object;
}

static void ir_finalize(JSObjectRef object)
{
	GSource *source = JSObjectGetPrivate(object);

	g_source_destroy(source);
}

static JSValueRef ir_get_onevent(JSContextRef context, JSObjectRef object,
		JSStringRef name, JSValueRef *exception)
{
	struct ir *priv = JSObjectGetPrivate(object);
	if (!priv) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return JSValueMakeNull(context);
	}
	return priv->callback;
}

static bool ir_set_onevent(JSContextRef context, JSObjectRef object,
		JSStringRef name, JSValueRef value, JSValueRef *exception)
{
	struct ir *priv = JSObjectGetPrivate(object);
	if (!priv) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return false;
	}

	priv->callback = JSValueToObject(context, value, exception);
	if (!priv->callback) {
		g_warning("js-irkey: failed to assign callback");
		return false;
	}

	return true;
}

static const JSStaticValue ir_properties[] = {
	{
		.name = "onevent",
		.getProperty = ir_get_onevent,
		.setProperty = ir_set_onevent,
		.attributes = kJSPropertyAttributeNone,
	}, {
	}
};

static const JSClassDefinition ir_classdef = {
	.className = "IR",
	.initialize = ir_initialize,
	.finalize = ir_finalize,
	.staticValues = ir_properties,
};

static JSObjectRef javascript_ir_create(
	JSContextRef js, JSClassRef class, struct javascript_userdata *data)
{
	GSource *source;

	source = ir_source_new(js);
	if (!source)
		return NULL;

	g_source_attach(source, g_main_loop_get_context(data->loop));
	g_source_unref(source);

	return JSObjectMake(js, class, source);
}

struct javascript_module javascript_ir = {
	.classdef = &ir_classdef,
	.create = javascript_ir_create,
};
