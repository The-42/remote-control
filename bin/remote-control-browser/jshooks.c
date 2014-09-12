/*
 * Copyright (C) 2013 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <string.h>
#include "jshooks.h"

#define MODNAME	"jshooks"

/* max FQDN length is 255 bytes (RFC 1035), plus own pre- and suffix */
#define JSHOOKS_LENGTH_FILENAME	264

/* name for file executed on each and every page. This needs to contain
 * a char considered illegal in a FQDN to avoid accidental double calls
 */
#define JSHOOKS_GLOBAL_FILENAME	"global-"

static gchar *get_fqdn_from_uri(const gchar *uri)
{
	GMatchInfo *match_info;
	gchar *fqdn = NULL;
	GRegex *regex;

	regex = g_regex_new(
			"([a-zA-Z0-9]{1}"
			"([a-zA-Z0-9\\-]*[a-zA-Z0-9])*)"
			"(\\.[a-zA-Z0-9]{1}([a-zA-Z0-9\\-]*[a-zA-Z0-9])*)*"
			"(\\.[a-zA-Z]{1}([a-zA-Z0-9\\-]*[a-zA-Z0-9])*)\\.?",
			0, 0, NULL);
	g_regex_match(regex, uri, 0, &match_info);

	if (g_match_info_matches(match_info))
		fqdn = g_strdup(g_match_info_fetch(match_info, 0));

	g_match_info_free(match_info);
	g_regex_unref(regex);

	return fqdn;
}

gchar **jshooks_determine_hooklist(const gchar *uri, const gchar *prefix)
{
	/* Generates a list of possible javascript hook files for a
	 * given URI. The file names are determined from the FQDN. The
	 * list is expected to be executed in order and therefore
	 * sorted:
	 *  - global file
	 *  - hostname without "www." and without TLD (1st level only)
	 *  - full hostname without "www."
	 * If a TLD somehow went missing, the middle part is left out.
	 */
	gchar hookname[JSHOOKS_LENGTH_FILENAME];
	gchar **hooklist = g_new(gchar *, 4);
	gchar *last_dot = NULL;
	gchar *fqdn_start;
	uint idx = 0;
	gchar *fqdn;
	gchar *fdot;

	if (!hooklist)
		return NULL;

	fqdn = get_fqdn_from_uri(uri);
	if (!fqdn) {
		g_warning("%s: FQDN extraction failed! URI was: %s",
				MODNAME, uri);
		hooklist[0] = NULL;
		return hooklist;
	}

	/* strip 'www.' since this particular string doesn't have any
	 * meaning for identification nowadays
	 */
	if (strlen(fqdn) > 4 && g_str_has_prefix(fqdn, "www."))
		fqdn_start = &fqdn[4];
	else
		fqdn_start = fqdn;

	/* strip TLD part to have a single script useable for several
	 * TLDs of the same page. Note that this does not account for
	 * 2nd-level TLDs like co.uk, com.au etc.
	 */
	fdot = g_strdup(fqdn_start);
	last_dot = g_strrstr(fdot, ".");
	if (last_dot) {
		*last_dot = '\0';
		g_snprintf(hookname, sizeof(hookname), "%s-%s.js", prefix, fdot);
		hooklist[idx++] = g_strndup(hookname, sizeof(hookname));
	}

	g_snprintf(hookname, sizeof(hookname), "%s-%s.js", prefix, fqdn_start);
	hooklist[idx++] = g_strndup(hookname, sizeof(hookname));

	g_snprintf(hookname, sizeof(hookname), "%s-%s.js",
				prefix, JSHOOKS_GLOBAL_FILENAME);
	hooklist[idx++] = g_strndup(hookname, sizeof(hookname));
	hooklist[idx++] = NULL;

	g_free(fdot);
	g_free(fqdn);

	return hooklist;
}

void jshooks_execute_jscript(JSContextRef js_context, gchar *content,
		gchar* sname)
{
	JSStringRef js_content = JSStringCreateWithUTF8CString(content);
	JSStringRef js_name = JSStringCreateWithUTF8CString(sname);
	JSValueRef err = NULL;
	char err_msg[255];

	JSEvaluateScript(js_context, js_content, NULL, js_name, 0, &err);
	JSStringRelease(js_content);
	JSStringRelease(js_name);

	if (err) {
		JSStringRef sr = JSValueToStringCopy(js_context, err, NULL);
		JSStringGetUTF8CString(sr, err_msg, sizeof(err_msg));
		g_warning("script %s failed --- %s ---", sname, err_msg);
		JSStringRelease(sr);
	}
}
