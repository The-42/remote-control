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

#include <errno.h>
#include <math.h>
#include <string.h>

#include "find-device.h"
#include "javascript.h"
#include "javascript-output.h"

#define CONFIG_GROUP "outputs"
#define MAX_OUTPUT_CHANNELS 16

struct js_output_channel_set {
	double value;
	unsigned duration;
};

struct js_output_channel {
	const struct js_output_type *type;
	char *name;
	struct js_output *output;
	struct js_output_channel_set *set;
	unsigned set_count;
	unsigned set_pos;
	guint timeout_id;
};

extern const struct js_output_type js_output_sysfs;

static const struct js_output_type * const output_types[] = {
	&js_output_sysfs,
	NULL,
};

static struct js_output_channel channels[MAX_OUTPUT_CHANNELS];
static unsigned channels_count = 0;

static bool js_output_set_value(
	JSContextRef context, JSObjectRef object,
	JSStringRef name, JSValueRef value,
	JSValueRef *exception)
{
	struct js_output_channel *channel = JSObjectGetPrivate(object);
	double dval;
	int err;

	if (!channel) {
		javascript_set_exception_text(context, exception,
			"object not valid, context switched?");
		return true;
	}

	dval = JSValueToNumber(context, value, exception);
	if (isnan(dval))
		return true;

	err = channel->type->set(channel->output, dval);
	if (err)
		javascript_set_exception_text(context, exception,
			"failed to set output value");
	return true;
}

static JSValueRef js_output_get_value(
	JSContextRef context, JSObjectRef object,
	JSStringRef name, JSValueRef *exception)
{
	struct js_output_channel *channel = JSObjectGetPrivate(object);
	double dval;
	int err;

	if (!channel) {
		javascript_set_exception_text(context, exception,
			"object not valid, context switched?");
		return NULL;
	}

	err = channel->type->get(channel->output, &dval);
	if (err)
		javascript_set_exception_text(context, exception,
			"failed to get output value");

	return JSValueMakeNumber(context, dval);
}

static gboolean js_output_on_set_done(gpointer user_data)
{
	struct js_output_channel *channel = user_data;
	struct js_output_channel_set *set;
	int err;

	channel->timeout_id = 0;

	if (!channel->set || channel->set_pos >= channel->set_count)
		return G_SOURCE_REMOVE;

	set = &channel->set[channel->set_pos];

	err = channel->type->set(channel->output, set->value);
	if (err) {
		g_warning("%s: Failed to set output %s to %g",
			__func__, channel->name, set->value);
		return G_SOURCE_REMOVE;
	}

	channel->set_pos += 1;

	if (channel->set_pos <= channel->set_count)
		channel->timeout_id = g_timeout_add(
			set->duration, js_output_on_set_done, channel);

	return G_SOURCE_REMOVE;
}

static const JSStaticValue js_output_properties[] = {
	{
		.name = "value",
		.getProperty = js_output_get_value,
		.setProperty = js_output_set_value,
		.attributes = kJSPropertyAttributeDontDelete,
	},
	{}
};

static JSValueRef js_output_set(
	JSContextRef context, JSObjectRef function,
	JSObjectRef object, size_t argc, const JSValueRef argv[],
	JSValueRef *exception)
{
	struct js_output_channel *channel = JSObjectGetPrivate(object);
	int i;

	if (!channel) {
		javascript_set_exception_text(context, exception,
			"object not valid, context switched?");
		return NULL;
	}

	/* We need an even number of arguments */
	if ((argc & 1) == 0) {
		javascript_set_exception_text(context, exception,
			"invalid argument count");
		return NULL;
	}

	/* Parse the value,duration pairs */
	channel->set_count = channel->set_pos = 0;
	channel->set = g_realloc_n(
		channel->set, (argc + 1) / 2, sizeof(*channel->set));

	for (i = 0; i < argc; i += 2) {
		struct js_output_channel_set *set =
			&channel->set[channel->set_count];
		set->value = JSValueToNumber(context, argv[i], exception);
		if (isnan(set->value)) {
			javascript_set_exception_text(context, exception,
				"value must be a number");
			return NULL;
		}
		if (i + 1 < argc) {
			double duration = JSValueToNumber(
				context, argv[i + 1], exception);
			if (isnan(duration) || duration < 0.0) {
				javascript_set_exception_text(
					context, exception,
					"duration must be a positive number");
				return NULL;
			}
			set->duration = duration;
		} else
			set->duration = 0;
		channel->set_count += 1;
	}

	/* Clear any pending timeout */
	if (channel->timeout_id) {
		g_source_remove(channel->timeout_id);
		channel->timeout_id = 0;
	}

	/* (re)start the process */
	js_output_on_set_done(channel);

	return JSValueMakeNull(context);
}

static const JSStaticFunction js_output_functions[] = {
	{
		.name = "set",
		.callAsFunction = js_output_set,
		.attributes = kJSPropertyAttributeDontDelete,
	},
	{}
};

static const JSClassDefinition js_output_classdef = {
	.className = "Output",
	.staticValues = js_output_properties,
	.staticFunctions = js_output_functions,
};

static JSObjectRef js_output_create(
	JSContextRef js, JSClassRef class,
	struct javascript_userdata *user_data)
{
	JSObjectRef root;
	int i;

	root = JSObjectMake(js, NULL, NULL);
	if (!root)
		return NULL;

	for (i = 0; i < channels_count; i++) {
		JSObjectRef channel;
		if (channels[i].type->prepare) {
			int err = channels[i].type->prepare(channels[i].output);
			if (err < 0) {
				g_warning("%s: Failed to prepare output %s",
					__func__, channels[i].name);
				continue;
			}
		}
		channel = JSObjectMake(js, class, &channels[i]);
		if (channel)
			javascript_object_set_property(
				js, root, channels[i].name, channel,
				kJSPropertyAttributeDontDelete |
				kJSPropertyAttributeReadOnly,
				NULL);
	}

	return root;
}

static const struct js_output_type* js_output_type_find(const char *name)
{
	int i;

	for (i = 0; output_types[i]; i++)
		if (!strcmp(name, output_types[i]->name))
			return output_types[i];

	return NULL;
}

char *js_output_config_get_string(
	GKeyFile *config, const char *name, const char *key)
{
	char *key_name;
	char *value;

	key_name = g_strdup_printf("%s-%s", name, key);
	if (!key_name)
		return NULL;

	value = g_key_file_get_string(config, CONFIG_GROUP, key_name, NULL);
	g_free(key_name);

	return value;
}

char **js_output_config_get_string_list(
	GKeyFile *config, const char *name, const char *key)
{
	char *key_name;
	char **value;

	key_name = g_strdup_printf("%s-%s", name, key);
	if (!key_name)
		return NULL;

	value = g_key_file_get_string_list(
		config, CONFIG_GROUP, key_name, NULL, NULL);
	g_free(key_name);

	return value;
}

double js_output_config_get_double(
	GKeyFile *config, const char *name, const char *key)
{
	char *key_name;
	double value;
	GError *err;

	key_name = g_strdup_printf("%s-%s", name, key);
	if (!key_name)
		return NAN;

	value = g_key_file_get_double(config, CONFIG_GROUP, key_name, &err);
	g_free(key_name);

	return err ? NAN : value;
}

int js_output_init_channel(GKeyFile *config, const char *name)
{
	const struct js_output_type *type;
	struct js_output *output = NULL;
	char *type_name;
	int err;

	if (channels_count >= MAX_OUTPUT_CHANNELS) {
		g_warning("%s: Too many outputs, can't configure %s",
			__func__, name);
		return -ENOMEM;
	}

	type_name = js_output_config_get_string(config, name, "type");
	if (!type_name) {
		g_warning("%s: Type of output %s isn't defined",
			__func__, name);
		return -EINVAL;
	}

	type = js_output_type_find(type_name);
	g_free(type_name);
	if (!type) {
		g_warning("%s: Output %s has an unknown type: %s",
			__func__, name, type_name);
		return -EINVAL;
	}

	err = type->create(config, name, &output);
	if (err) {
		g_warning("%s: Failed to create output %s",
			__func__, name);
		return err;
	}

	memset(&channels[channels_count], 0, sizeof(*channels));
	channels[channels_count].type = type;
	channels[channels_count].name = strdup(name);
	channels[channels_count].output = output;
	channels_count += 1;

	return 0;
}

int js_output_init(GKeyFile *config)
{
	gchar **names;
	int i, err;

	names = g_key_file_get_string_list(
		config, CONFIG_GROUP, "names", NULL, NULL);
	if (!names)
		return 0;

	for (i = 0; names[i] != NULL; i++) {
		err = js_output_init_channel(config, names[i]);
		if (err)
			g_warning("%s: Failed to init output %s",
				__func__, names[i]);
	}

	g_strfreev(names);
	return 0;
}

struct javascript_module javascript_output = {
	.classdef = &js_output_classdef,
	.init = js_output_init,
	.create = js_output_create,
};
