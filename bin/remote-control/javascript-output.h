/*
 * Copyright (C) 2014 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef JAVASCRIPT_OUTPUT_H
#define JAVASCRIPT_OUTPUT_H 1

#include <glib.h>

/** The output struct content is defined by the implementations */
struct js_output;

struct js_output_type {
	/** Output type name */
	const char *name;
	/**
	 * This callback should create a new output
	 *
	 * @param config[in] The config file
	 * @param name[in]   The output name, needed to read the config
	 * @param outp[out]  The newly allocated output
	 * @return A negative error code in case of error, 0 otherwise
	 */
	int (*create)(GKeyFile *config, const char *name,
		struct js_output **outp);

	/**
	 * Prepare an output, this is called before creating the javascript
	 * object to allow for some late init. Unlike create this callback
	 * might get called many times.
	 *
	 * @param out The output to prepare for use
	 * @return A negative error code in case of error, 0 otherwise
	 */
	int (*prepare)(struct js_output *out);

	/**
	 * Set an output to the given value
	 *
	 * @param out   The output to set
	 * @param value The new output value
	 * @return A negative error code in case of error, 0 otherwise
	 */
	int (*set)(struct js_output *out, double value);

	/**
	 * Get an the current value of an output
	 *
	 * @param out[in]    The output to get
	 * @param value[out] The value read from the output
	 * @return A negative error code in case of error, 0 otherwise
	 */
	int (*get)(struct js_output *out, double* value);
};

/**
 * Helper function to read an output config key as string
 *
 * @param config The config file
 * @param name   The output name
 * @param key    The config key name
 * @return A newly allocated string or NULL in case of error
 *
 */
char *js_output_config_get_string(
	GKeyFile *config, const char *name, const char *key);

/**
 * Helper function to read an output config key as string list
 *
 * @param config The config file
 * @param name   The output name
 * @param key    The config key name
 * @return A newly allocated string list or NULL in case of error
 *
 */
char **js_output_config_get_string_list(
	GKeyFile *config, const char *name, const char *key);

/**
 * Helper function to read an output config key as double
 *
 * @param config The config file
 * @param name   The output name
 * @param key    The config key name
 * @return The read value of NaN in case of error
 *
 */
double js_output_config_get_double(
	GKeyFile *config, const char *name, const char *key);


#endif /* JAVASCRIPT_OUTPUT_H */
