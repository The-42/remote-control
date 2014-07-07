/*
 * Copyright (C) 2014 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "javascript.h"

#include <sys/ioctl.h>
#include <linux/fb.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

struct js_fb_data {
	char *name;
	char *device;
	int fd;
	int width;
	int height;
	int stride;
};

#define JS_FB_MAX                       16
#define JS_FB_CONFIG_GROUP              "framebuffer "
#define JS_FB_CLEAR_CHAR                ' '
#define JS_FB_WRITE_AGAIN_DELAY         100

#define JS_FB_ERR_INVALID_FRAMEBUFFER   "framebuffer device not functional"
#define JS_FB_ERR_FAILED_TO_SEEK        "failed to seek to position"
#define JS_FB_ERR_FAILED_TO_WRITE       "failed to write text"
#define JS_FB_ERR_FAILED_TO_ALLOC       "failed to alloc memory"
#define JS_FB_ERR_FAILED_TO_BLANK       "failed to blank display"

static struct js_fb_data js_fb[JS_FB_MAX];
static unsigned js_fb_count = 0;

static void js_fb_reset_fb(struct js_fb_data *data)
{
	if (-1 != data->fd)
		close(data->fd);
	data->fd = -1;
	data->width = 0;
	data->height = 0;
	data->stride = 0;
}

static int js_fb_prepare_fb(struct js_fb_data *data)
{
	struct fb_var_screeninfo vinfo;
	struct fb_fix_screeninfo finfo;
	int ret = -EINVAL;

	if (-1 != data->fd)
		return 0; /* already initialized */

	data->fd = open(data->device, O_RDWR);
	if (-1 == data->fd) {
		ret = -errno;
		g_warning("%s: Failed to open %s for framebuffer %s (%d)",
				__func__, data->device, data->name, errno);
		js_fb_reset_fb(data);
		return ret;
	}
	if (-1 == ioctl(data->fd, FBIOGET_FSCREENINFO, &finfo)) {
		ret = -errno;
		g_warning("%s: Failed to get finfo for framebuffer %s (%d)",
				__func__, data->name, errno);
		js_fb_reset_fb(data);
		return ret;
	}
	if (FB_TYPE_TEXT != finfo.type) {
		g_warning("%s: Framebuffer %s is not of type text (%d)",
				__func__, data->name, finfo.type);
		js_fb_reset_fb(data);
		return ret;
	}
	if (-1 == ioctl(data->fd, FBIOGET_VSCREENINFO, &vinfo)) {
		ret = -errno;
		g_warning("%s: Failed to get vinfo for framebuffer %s (%d)",
				__func__, data->name, errno);
		js_fb_reset_fb(data);
		return ret;
	}

	data->width = vinfo.xres;
	data->height = vinfo.yres;
	data->stride = finfo.line_length;

	return 0;
}

static int js_fb_write_text(struct js_fb_data *fb, int x, int y,
		const char *text, JSContextRef context, JSValueRef *exception)
{
	int ret, len = strlen(text);

	ret = lseek(fb->fd, y * fb->stride + x, SEEK_SET);
	if (-1 == ret) {
		ret = -errno;
		javascript_set_exception_text(context, exception,
				JS_FB_ERR_FAILED_TO_SEEK);
		js_fb_reset_fb(fb);
		return ret;
	}
	ret = write(fb->fd, text, len);
	if (-1 == ret) {
		ret = -errno;
		javascript_set_exception_text(context, exception,
				JS_FB_ERR_FAILED_TO_WRITE);
		js_fb_reset_fb(fb);
		return ret;
	}

	return ret;
}

static int js_fb_check_object(JSContextRef context, struct js_fb_data *fb,
		JSValueRef *exception)
{
	int ret;

	if (!fb) {
		javascript_set_exception_text(context, exception,
				JS_ERR_INVALID_OBJECT_TEXT);
		return -EINVAL;
	}
	ret = js_fb_prepare_fb(fb);
	if (ret)
		javascript_set_exception_text(context, exception,
				JS_FB_ERR_INVALID_FRAMEBUFFER);
	return ret;
}

/*
 *  Module properties
 */

static JSValueRef js_fb_get_width(JSContextRef context, JSObjectRef object,
		JSStringRef name, JSValueRef *exception)
{
	struct js_fb_data *fb = JSObjectGetPrivate(object);

	if (js_fb_check_object(context, fb, exception))
		return NULL;

	return JSValueMakeNumber(context, fb->width);
}

static JSValueRef js_fb_get_height(JSContextRef context, JSObjectRef object,
		JSStringRef name, JSValueRef *exception)
{
	struct js_fb_data *fb = JSObjectGetPrivate(object);

	if (js_fb_check_object(context, fb, exception))
		return NULL;

	return JSValueMakeNumber(context, fb->height);
}

static const JSStaticValue js_fb_properties[] = {
	{
		.name = "width",
		.getProperty = js_fb_get_width,
		.attributes = kJSPropertyAttributeDontDelete |
				kJSPropertyAttributeReadOnly,
	},
	{
		.name = "height",
		.getProperty = js_fb_get_height,
		.attributes = kJSPropertyAttributeDontDelete |
				kJSPropertyAttributeReadOnly,
	},
	{}
};

/*
 *  Module funtions
 */

static JSValueRef js_fb_set_text(JSContextRef context, JSObjectRef function,
		JSObjectRef object, size_t argc, const JSValueRef argv[],
		JSValueRef *exception)
{
	struct js_fb_data *fb = JSObjectGetPrivate(object);
	int count, pos_x = 0, pos_y = 0;
	JSValueRef ret = NULL;
	char *text = NULL;

	if (js_fb_check_object(context, fb, exception))
		goto cleanup;

	/* Usage setText(text[,x[,y]]) */

	switch (argc) {
	case 3:
		if (javascript_int_from_number(context, argv[2], 0,
				UINT16_MAX, &pos_y, exception)) {
			javascript_set_exception_text(context, exception,
					JS_ERR_INVALID_NUMBER);
			goto cleanup;
		}
		/* no break */
	case 2:
		if (javascript_int_from_number(context, argv[1], 0,
				UINT16_MAX, &pos_x, exception)) {
			javascript_set_exception_text(context, exception,
					JS_ERR_INVALID_NUMBER);
			goto cleanup;
		}
		/* no break */
	case 1:
		text = javascript_get_string(context, argv[0], exception);
		if (!text)
			goto cleanup;
		break;
	default:
		javascript_set_exception_text(context, exception,
				JS_ERR_INVALID_ARG_COUNT);
		goto cleanup;
	}

	count = js_fb_write_text(fb, pos_x, pos_y, text, context, exception);
	if (strlen(text) != count) {
		javascript_set_exception_text(context, exception,
				JS_FB_ERR_FAILED_TO_WRITE);
		goto cleanup;
	}

	ret = JSValueMakeBoolean(context, TRUE);

cleanup:
	if (text)
		g_free(text);
	return ret;
}

static JSValueRef js_fb_clear_text(JSContextRef context, JSObjectRef function,
		JSObjectRef object, size_t argc, const JSValueRef argv[],
		JSValueRef *exception)
{
	struct js_fb_data *fb = JSObjectGetPrivate(object);
	JSValueRef ret = NULL;
	bool failed = FALSE;
	char *fill = NULL;
	int count, i = 0;

	if (js_fb_check_object(context, fb, exception))
		goto cleanup;

	/* Usage clearText() */
	if (argc != 0) {
		javascript_set_exception_text(context, exception,
				JS_ERR_INVALID_ARG_COUNT);
		goto cleanup;
	}

	fill = g_strnfill(fb->width, JS_FB_CLEAR_CHAR);
	if (!fill) {
		javascript_set_exception_text(context, exception,
				JS_FB_ERR_FAILED_TO_ALLOC);
		goto cleanup;
	}

	while (i < fb->height) {
		count = js_fb_write_text(fb, 0, i++, fill, context, exception);
		if (count != fb->width)
			failed = TRUE;
		 /* If failed, keep on trying to clear remaining lines */
	}
	if (failed) {
		javascript_set_exception_text(context, exception,
				JS_FB_ERR_FAILED_TO_WRITE);
		goto cleanup;
	}

	ret = JSValueMakeBoolean(context, TRUE);

cleanup:
	if (fill)
		g_free(fill);
	return ret;
}

#define FB_BLANK(v, n) { .value = FB_BLANK_##v, .name = n }
static const struct javascript_enum js_fb_blank_enum[] = {
	FB_BLANK(UNBLANK, "unblank"),
	FB_BLANK(NORMAL, "normal"),
	FB_BLANK(VSYNC_SUSPEND, "vsync"),
	FB_BLANK(HSYNC_SUSPEND, "hsync"),
	FB_BLANK(POWERDOWN, "powerdown"),
};

static JSValueRef js_fb_set_blank(JSContextRef context, JSObjectRef function,
		JSObjectRef object, size_t argc, const JSValueRef argv[],
		JSValueRef *exception)
{
	struct js_fb_data *fb = JSObjectGetPrivate(object);
	JSValueRef ret = NULL;
	int blank = FB_BLANK_UNBLANK;

	if (js_fb_check_object(context, fb, exception))
		goto cleanup;

	/* Usage setBlank(true|false) or setBlank(value) */
	if (argc != 1) {
		javascript_set_exception_text(context, exception,
				JS_ERR_INVALID_ARG_COUNT);
		goto cleanup;
	}
	if (JSValueIsBoolean(context, argv[0])) {
		if (JSValueToBoolean(context, argv[0]))
			blank = FB_BLANK_POWERDOWN;
	} else if (javascript_enum_from_string(context, js_fb_blank_enum,
			argv[0], &blank, exception)) {
		goto cleanup;
	}

	if (-1 == ioctl(fb->fd, FBIOBLANK, blank)) {
		javascript_set_exception_text(context, exception,
				JS_FB_ERR_FAILED_TO_BLANK);
		js_fb_reset_fb(fb);
		goto cleanup;
	}

	ret = JSValueMakeBoolean(context, TRUE);

cleanup:
	return ret;
}

static const JSStaticFunction js_fb_functions[] = {
	{
		.name = "setText",
		.callAsFunction = js_fb_set_text,
		.attributes = kJSPropertyAttributeDontDelete,
	},
	{
		.name = "clearText",
		.callAsFunction = js_fb_clear_text,
		.attributes = kJSPropertyAttributeDontDelete,
	},
	{
		.name = "setBlank",
		.callAsFunction = js_fb_set_blank,
		.attributes = kJSPropertyAttributeDontDelete,
	},
	{}
};

/*
 *  Module funtions
 */

static int js_fb_init_fb(GKeyFile *config, const char *name,
		struct js_fb_data *data)
{
	data->device = javascript_config_get_string(config, JS_FB_CONFIG_GROUP,
			name, "device");
	if (!data->device) {
		g_warning("%s: Failed to get device for framebuffer %s",
				__func__, name);
		return -EINVAL;
	}
	data->name = g_strdup(name);
	data->fd = -1;
	js_fb_reset_fb(data);

	return 0;
}

static int js_fb_init(GKeyFile *config)
{
	gchar **names;
	int i;

	names = javascript_config_get_groups(config, JS_FB_CONFIG_GROUP);
	if (!names)
		return 0;

	for (i = 0; names[i] != NULL; i++) {
		if (js_fb_count >= JS_FB_MAX) {
			g_warning("%s: Too many framebuffers, ignoring %s",
					__func__, names[i]);
			continue;
		}

		memset(&js_fb[js_fb_count], 0, sizeof(*js_fb));

		if (!js_fb_init_fb(config, names[i], &js_fb[js_fb_count]))
			js_fb_count++;
	}

	g_strfreev(names);
	return 0;
}

static JSObjectRef js_fb_create(JSContextRef js, JSClassRef class,
		struct javascript_userdata *user_data)
{
	JSObjectRef root;
	int i;

	root = JSObjectMake(js, NULL, NULL);
	if (!root)
		return NULL;

	for (i = 0; i < js_fb_count; i++) {
		JSObjectRef ref = JSObjectMake(js, class, &js_fb[i]);
		if (!ref)
			continue;
		javascript_object_set_property(js, root, js_fb[i].name, ref,
				kJSPropertyAttributeDontDelete |
					kJSPropertyAttributeReadOnly,
				NULL);
	}
	return root;
}

/*
 *  Module definition
 */

static const JSClassDefinition js_fb_classdef = {
	.className = "FB",
	.staticValues = js_fb_properties,
	.staticFunctions = js_fb_functions,
};

struct javascript_module javascript_fb = {
	.classdef = &js_fb_classdef,
	.init = js_fb_init,
	.create = js_fb_create,
};
