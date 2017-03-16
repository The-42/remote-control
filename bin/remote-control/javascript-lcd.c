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
#include <ctype.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>

#include "javascript.h"

#define DEFAULT_TIMEOUT        100
#define DEFAULT_READ_TIMEOUT   DEFAULT_TIMEOUT
#define DEFAULT_WRITE_TIMEOUT  DEFAULT_TIMEOUT

#define DEFAULT_LCD_PORT_CHROMIUM "/dev/ttyS1"

#define COMMAND_POWER_OFF      0
#define COMMAND_POWER_ON       1
#define COMMAND_VOLUME_MUTE    2
#define COMMAND_VOLUME_UNMUTE  3
#define COMMAND_VOLUME_SET     4


struct lcd_command {
	guint id;
	guint length;
	const char *cmd;
};

struct lcd {
	GSource source;
	GPollFD *tty;

	JSContextRef context;
	JSObjectRef receive_cb;
	JSObjectRef thisptr;
};

static const struct lcd_command lg_tv_commands[] = {
	{ COMMAND_POWER_OFF,     5, "ka 1 0\r" },
	{ COMMAND_POWER_ON,      5, "ka 1 1\r" },
	{ COMMAND_VOLUME_MUTE,   5, "ke 1 0\r" },
	{ COMMAND_VOLUME_UNMUTE, 5, "ke 1 1\r" },
	{ COMMAND_VOLUME_SET,    5, "kf 1 %02x\r" },
};

#ifdef DEBUG
static void hexdump(uint8_t *buf, size_t len)
{
	const size_t rowsize = 16;
	const uint8_t *ptr = buf;
	FILE *fp = stdout;
	size_t i, j;

	for (j = 0; j < len; j += rowsize) {

		for (i = 0; i < rowsize; i++) {
			if ((j + i) < len)
				fprintf(fp, "%s%02x", i ? " " : "", ptr[j + i]);
			else
				fprintf(fp, "%s  ", i ? " " : "");
		}

		fprintf(fp, " | ");

		for (i = 0; i < rowsize; i++) {
			if ((j + i) < len) {
				if (isprint(ptr[j + i]))
					fprintf(fp, "%c", ptr[j + i]);
				else
					fprintf(fp, ".");
			} else {
				fprintf(fp, " ");
			}
		}

		fprintf(fp, " |\n");
	}
}
#else
#define hexdump(b,l)
#endif

static ssize_t read_all(int fd, uint8_t *buffer, size_t count, ulong timeout)
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

			g_debug("   select failed: %d (%s)", errno, g_strerror(errno));
			return -errno;
		}

		if (err == 0) {
			g_debug("   timeout, read=%zu, count=%zu", pos, count);
			if (pos > 0)
				return pos;
			return -ETIMEDOUT;
		}

		if (FD_ISSET(fd, &rfds)) {
			num = read(fd, buffer + pos, count - pos);
			if (num < 0) {
				if (errno == EINTR)
					continue;
				g_debug("   read failed: %d (%s)", errno, g_strerror(errno));
				return -errno;
			}

			pos += num;
		}
	}

	return pos;
}

#ifdef CHECK_DATA
static int parse_response(guint8 *buffer, guint length)
{
	const char *message = (const char*)buffer;

	if (length < 1)
		return -EINVAL;

	if (strlen(message) < 1)
		return -EINVAL;

	return -ENOSYS;
}
#endif

static int lcd_report(struct lcd *lcd, uint8_t *data, int length)
{
	JSValueRef exception = NULL;
	JSValueRef arguments[1];
	JSValueRef *array;
	int i;

	g_return_val_if_fail(lcd->context != NULL, -EINVAL);
	g_return_val_if_fail(data != NULL, -EINVAL);
	g_return_val_if_fail(length > 0, -EINVAL);

	hexdump(data, length);

	/* serial object has been used but callback has not been set */
	if (lcd->receive_cb == NULL)
		return 0;

	/* wrap the data into a array */
	array = g_newa(JSValueRef, length);
	for (i=0; i<length; i++)
		array[i] = JSValueMakeNumber(lcd->context, data[i]);

	arguments[0] = JSObjectMakeArray(lcd->context, length, array,
	                                 &exception);

	(void)JSObjectCallAsFunction(lcd->context, lcd->receive_cb,
			lcd->thisptr, G_N_ELEMENTS(arguments), arguments,
			&exception);
	if (exception) {
		g_warning(JS_LOG_CALLBACK_EXCEPTION, __func__);
		return -EFAULT;
	}
	return 0;
}

static gboolean lcd_source_prepare(GSource *source, gint *timeout)
{
	if (timeout)
		*timeout = -1;

	return FALSE;
}

static gboolean lcd_source_check(GSource *source)
{
	struct lcd *lcd = (struct lcd *)source;
	GPollFD *poll = lcd->tty;

	if (poll->revents & G_IO_IN)
		return TRUE;

	return FALSE;
}

static gboolean lcd_source_dispatch(GSource *source, GSourceFunc callback,
                                    gpointer user_data)
{
	struct lcd *lcd = (struct lcd *)source;
	GPollFD *poll = lcd->tty;
	guint8 buf[1024];
	ssize_t got;
	int err;

	got = read_all(poll->fd, buf, sizeof(buf), DEFAULT_READ_TIMEOUT);
	if (got < 0) {
		g_warning("%s: read failed: %zd", __func__, got);
		goto fail;
	}

	err = lcd_report(lcd, buf, got);
	if (err < 0) {
		if (err != -EFAULT)
			g_warning("%s: %s", __func__, g_strerror(-err));
		goto fail;
	}

fail:
	if (callback)
		return callback(user_data);

	return TRUE;
}

static void lcd_source_finalize(GSource *source)
{
	struct lcd *lcd = (struct lcd *)source;

	if (lcd->tty) {
		g_warning("%s: closing device", __func__);
		close(lcd->tty->fd);
		lcd->tty->fd = 0;
		g_free(lcd->tty);
		lcd->tty = NULL;
	}
}

static GSourceFuncs lcd_source_funcs = {
	.prepare = lcd_source_prepare,
	.check = lcd_source_check,
	.dispatch = lcd_source_dispatch,
	.finalize = lcd_source_finalize,
};

static int open_tty(const char *tty, int *fd)
{
	struct termios attr;
	int ret;

	ret = open(tty, O_RDWR | O_NOCTTY | O_NONBLOCK, 0);
	if (ret == -1) {
		g_warning("%s: failed to open device: [%s]",
			__func__, tty);
		return -errno;
	}
	*fd = ret;
	fcntl(*fd, F_SETFL, O_RDWR);

	memset(&attr, 0, sizeof(attr));
	ret = tcgetattr(*fd, &attr);
	if (ret != 0) {
		g_warning("%s: failed to get TTY settings: %s",
			__func__, g_strerror(errno));
		return -errno;
	}

	cfmakeraw(&attr);
	attr.c_cflag |= (B9600 | CS8 | CLOCAL);

//	cfsetspeed(*fd, B9600);
	ret = tcsetattr(*fd, TCSAFLUSH, &attr);
	if (ret < 0) {
		g_warning("%s: failed to set TTY settings: %s",
			__func__, g_strerror(errno));
		return -errno;
	}

	ret = tcflush(*fd, TCIFLUSH);
	if (ret < 0) {
		g_warning("%s: failed to flush TTY: %s",
			__func__, g_strerror(errno));
	}

	return 0;
}

static int lcd_setup_poll(struct lcd *lcd, int fd)
{
	g_return_val_if_fail(lcd != NULL, -EINVAL);
	g_return_val_if_fail(fd != 0, -EINVAL);

	lcd->tty = g_new0(GPollFD, 1);
	if (!lcd->tty) {
		close(fd);
		return -ENOMEM;
	}

	lcd->tty->events = G_IO_IN | G_IO_HUP | G_IO_ERR;
	lcd->tty->fd = fd;

	g_source_add_poll((GSource *)lcd, lcd->tty);
	return 0;
}

static GSource *lcd_source_new(JSContextRef context)
{
	static const char* ttys[] = {
		DEFAULT_LCD_PORT_CHROMIUM
	};

	GSource *source;
	struct lcd *lcd;
	int fd = 0;
	int ret;
	int i;

	source = g_source_new(&lcd_source_funcs, sizeof(*lcd));
	if (!source) {
		g_warning("%s: failed to allocate memory", __func__);
		return NULL;
	}

	lcd = (struct lcd *)source;
	lcd->context = context;
	lcd->receive_cb = NULL;

	for (i=0; i<G_N_ELEMENTS(ttys); i++) {
		ret = open_tty(ttys[i], &fd);
		if (ret < 0) {
			if (ret == -ENOENT)
				continue;
			g_warning("%s: failed to open TTY: %s",
				__func__, g_strerror(-ret));
			goto cleanup;
		}
		g_debug("%s: using port [%s]", __func__, ttys[i]);
		break;
	}

	ret = lcd_setup_poll(lcd, fd);
	if (ret < 0) {
		g_warning("%s: failed to create polling fd: %s",
			__func__, g_strerror(-ret));
		goto cleanup;
	}

	return source;

cleanup:
	g_source_unref(source);
	return NULL;
}

static void lcd_initialize(JSContextRef context, JSObjectRef object)
{
	struct lcd *lcd = JSObjectGetPrivate(object);
	lcd->thisptr = object;
}

static void lcd_finalize(JSObjectRef object)
{
	struct lcd *lcd = JSObjectGetPrivate(object);

	if (lcd->receive_cb)
		JSValueUnprotect(lcd->context, lcd->receive_cb);

	g_source_destroy(&lcd->source);
}


static const struct lcd_command* lcd_find_command(struct lcd *lcd, int command)
{
	const struct lcd_command *cmd = NULL;
	int i;

	for (i=0; i<G_N_ELEMENTS(lg_tv_commands); i++) {
		if (lg_tv_commands[i].id == command) {
			cmd = &lg_tv_commands[i];
			break;
		}
	}

	return cmd;
}

static int lcd_send_command(struct lcd *lcd, int command, int value)
{
	const struct lcd_command *cmd;
	GPollFD *poll = lcd->tty;
	guint8 *buf;
	int len;
	int ret;

	g_debug("> %s(lcd=%p, command=%d, value=%d)",
		__func__, lcd, command, value);

	cmd = lcd_find_command(lcd, command);
	if (!cmd) {
		g_debug("< %s() lcd_find_command: ENOSYS", __func__);
		return -ENOSYS;
	}

	len = (cmd->length * 2);
	buf = g_alloca(len);
	len = g_snprintf((char*)buf, len, cmd->cmd, value);

	hexdump(buf, len);

	ret = write(poll->fd, buf, len);
	if (ret < 0) {
		g_debug("< %s(): write: EIO", __func__);
		return -EIO;
	}

#ifdef CHECK_DATA
	ret = read_all(poll->fd, buf, cmd->length * 2, DEFAULT_READ_TIMEOUT);
	if (ret < 0) {
		g_debug("< %s(): read_all: EIO", __func__);
		return -EIO;
	}
	ret = parse_response(buf, ret);
	if (ret < 0) {
		g_debug("< %s(): parse_response: EINVAL", __func__);
		return -EINVAL;
	}
#endif
	g_debug("< %s(): OK", __func__);
	return 0;
}

/*
 * AvionicDesign.LCD.send(String command)
 */
static JSValueRef lcd_function_send(JSContextRef context,
                                    JSObjectRef function,
                                    JSObjectRef object,
                                    size_t argc, const JSValueRef argv[],
                                    JSValueRef *exception)
{
	struct lcd *priv = JSObjectGetPrivate(object);
	JSStringRef string;
	char *command;
	size_t length;
	ssize_t ret;

	if (!priv) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return JSValueMakeBoolean(context, FALSE);
	}

	if (argc != 1) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_ARG_COUNT);
		return JSValueMakeBoolean(context, FALSE);
	}

	if (!JSValueIsString(context, argv[0])) {
		javascript_set_exception_text(context, exception,
			"command is not a string");
		return JSValueMakeBoolean(context, FALSE);
	}

	string = JSValueToStringCopy(context, argv[0], exception);
	if (!string)
		return JSValueMakeBoolean(context, FALSE);

	length = JSStringGetMaximumUTF8CStringSize(string);
	command = g_alloca(length+1);

	length = JSStringGetUTF8CString(string, command, length);
	JSStringRelease(string);

	hexdump(command, length);

	ret = write(priv->tty->fd, command, length);
	if (ret < 0) {
		javascript_set_exception_text(context, exception,
			"write failed");
		return JSValueMakeBoolean(context, FALSE);
	}

	return JSValueMakeBoolean(context, TRUE);
}

/*
 * AvionicDesign.LCD.power(boolean on|off)
 */
static JSValueRef lcd_function_power(JSContextRef context,
                                     JSObjectRef function,
                                     JSObjectRef object,
                                     size_t argc, const JSValueRef argv[],
                                     JSValueRef *exception)
{
	struct lcd *lcd = JSObjectGetPrivate(object);
	gboolean enable;
	int cmd;
	int ret;

	if (!lcd) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return JSValueMakeBoolean(context, FALSE);
	}

	if (argc != 1) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_ARG_COUNT);
		return JSValueMakeBoolean(context, FALSE);
	}

	if (!JSValueIsBoolean(context, argv[0])) {
		javascript_set_exception_text(context, exception,
			"command is not a boolean");
		return JSValueMakeBoolean(context, FALSE);
	}

	enable = JSValueToBoolean(context, argv[0]);
	cmd = enable ? COMMAND_POWER_ON : COMMAND_POWER_OFF;

	ret = lcd_send_command(lcd, cmd, 0);
	if (ret < 0) {
		javascript_set_exception_text(context, exception,
			"write failed");
		return JSValueMakeBoolean(context, FALSE);
	}

	return JSValueMakeBoolean(context, TRUE);
}

/*
 * AvionicDesign.LCD.mute(boolean mute|unmute)
 */
static JSValueRef lcd_function_mute(JSContextRef context,
                                    JSObjectRef function,
                                    JSObjectRef object,
                                    size_t argc, const JSValueRef argv[],
                                    JSValueRef *exception)
{
	struct lcd *lcd = JSObjectGetPrivate(object);
	gboolean mute;
	int cmd;
	int ret;

	if (!lcd) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return JSValueMakeBoolean(context, FALSE);
	}

	if (argc != 1) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_ARG_COUNT);
		return JSValueMakeBoolean(context, FALSE);
	}

	if (!JSValueIsBoolean(context, argv[0])) {
		javascript_set_exception_text(context, exception,
			"command is not a boolean");
		return JSValueMakeBoolean(context, FALSE);
	}

	mute = JSValueToBoolean(context, argv[0]);
	cmd = mute ? COMMAND_VOLUME_MUTE : COMMAND_VOLUME_UNMUTE;

	ret = lcd_send_command(lcd, cmd, 0);
	if (ret < 0) {
		javascript_set_exception_text(context, exception,
			"write failed");
		return JSValueMakeBoolean(context, FALSE);
	}

	return JSValueMakeBoolean(context, TRUE);
}

static const JSStaticFunction lcd_functions[] = {
	{
		.name = "send",
		.callAsFunction = lcd_function_send,
		.attributes = kJSPropertyAttributeNone,
	}, {
		.name = "power",
		.callAsFunction = lcd_function_power,
		.attributes = kJSPropertyAttributeNone,
	}, {
		.name = "mute",
		.callAsFunction = lcd_function_mute,
		.attributes = kJSPropertyAttributeNone,
	}, {
	}

};

static JSValueRef lcd_get_onevent(JSContextRef context, JSObjectRef object,
                                  JSStringRef name, JSValueRef *exception)
{
	struct lcd *priv = JSObjectGetPrivate(object);
	if (!priv) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return JSValueMakeNull(context);
	}
	return priv->receive_cb;
}

static bool lcd_set_onevent(JSContextRef context, JSObjectRef object,
                            JSStringRef name, JSValueRef value,
                            JSValueRef *exception)
{
	struct lcd *priv = JSObjectGetPrivate(object);
	if (!priv) {
		javascript_set_exception_text(context, exception,
			JS_ERR_INVALID_OBJECT_TEXT);
		return false;
	}

	if (priv->receive_cb)
		JSValueUnprotect(context, priv->receive_cb);

	if (JSValueIsNull(context, value)) {
		priv->receive_cb = NULL;
		return true;
	}

	priv->receive_cb = JSValueToObject(context, value, exception);
	if (!priv->receive_cb) {
		g_warning("%s: failed to assign callback", __func__);
		return false;
	}
	JSValueProtect(context, priv->receive_cb);

	return true;
}

static const JSStaticValue lcd_properties[] = {
	{
		.name = "onevent",
		.getProperty = lcd_get_onevent,
		.setProperty = lcd_set_onevent,
		.attributes = kJSPropertyAttributeNone,
	}, {
	}
};

static const JSClassDefinition lcd_classdef = {
	.className = "LCD",
	.initialize = lcd_initialize,
	.finalize = lcd_finalize,
	.staticValues = lcd_properties,
	.staticFunctions = lcd_functions
};

static JSObjectRef javascript_lcd_create(
	JSContextRef js, JSClassRef class, struct javascript_userdata *data)
{
	GSource *source;

	source = lcd_source_new(js);
	if (!source)
		return NULL;

	g_source_attach(source, g_main_loop_get_context(data->loop));
	g_source_unref(source);

	return JSObjectMake(js, class, source);
}

struct javascript_module javascript_lcd = {
	.classdef = &lcd_classdef,
	.create = javascript_lcd_create,
};
