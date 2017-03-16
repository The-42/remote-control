/*
 Copyright (C) 2009-2012 Christian Dywan <christian@twotoasts.de>
 Copyright (C) 2009-2012 Alexander Butenko <a.butenka@gmail.com>
 Copyright (C) 2012-2013 Avionic Design GmbH

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "webkit-browser.h"
#include <webkit/webkit.h>
#include <glib/gstdio.h>
#include <stdlib.h>
#include <glib.h>
#include <glib-object.h>
#include "config.h"
#if HAVE_UNISTD_H
    #include <unistd.h>
#endif

#include <JavaScriptCore/JavaScript.h>
#include <string.h>

#include "adblock.h"
#include "guri.h"

#define SIGNATURE_SIZE 8
#define USE_PATTERN_MATCHING 1

#define DEFAULT_TEMP_DIR "/tmp"

static GHashTable* pattern;
static GHashTable* keys;
static GHashTable* optslist;
static GHashTable* urlcache;
static GHashTable* blockcssprivate;
static GHashTable* navigationwhitelist;
static GString* blockcss;
static GList* update_list;
static gboolean update_done;

static const gchar *filter_list[] = {
	"https://easylist-downloads.adblockplus.org/easylist_noelemhide.txt",
	"https://easylist-downloads.adblockplus.org/fanboy-annoyance.txt",
	NULL
};

static void
adblock_parse_file(gchar* path);

static gboolean
adblock_file_is_up_to_date(gchar* path);

static void
adblock_reload_rules(gboolean custom_only);

static GString*
adblock_fixup_regexp(const gchar* prefix, gchar* src);

static gchar*
adblock_build_js(const gchar* uri)
{
	GString* subdomain;
	gchar** subdomains;
	const gchar* style;
	const gchar* host;
	int blockscnt = 0;
	GString* code;
	int cnt = 0;
	GURI *guri;

	guri = g_uri_new(uri);
	if (!guri)
		return NULL;

	host = g_uri_get_host(guri);
	subdomains = g_strsplit(host, ".", -1);
	g_object_unref(guri);

	if (!subdomains)
		return NULL;

	code = g_string_new (
		"window.addEventListener ('DOMContentLoaded',"
		"function () {"
		"   if (document.getElementById('madblock'))"
		"       return;"
		"   public = '"
	);

	cnt = g_strv_length(subdomains) - 1;
	subdomain = g_string_new(subdomains [cnt]);
	g_string_prepend_c(subdomain, '.');
	cnt--;

	while (cnt >= 0) {
		g_string_prepend(subdomain, subdomains[cnt]);
		if ((style = g_hash_table_lookup(blockcssprivate, subdomain->str))) {
			g_string_append(code, style);
			g_string_append_c(code, ',');
			blockscnt++;
		}
		g_string_prepend_c(subdomain, '.');
		cnt--;
	}
	g_string_free(subdomain, TRUE);
	g_strfreev(subdomains);

	if (blockscnt == 0)
		return g_string_free(code, TRUE);

	g_string_append (code,
		"   zz-non-existent {display: none !important}';"
		"   var mystyle = document.createElement('style');"
		"   mystyle.setAttribute('type', 'text/css');"
		"   mystyle.setAttribute('id', 'madblock');"
		"   mystyle.appendChild(document.createTextNode(public));"
		"   var head = document.getElementsByTagName('head')[0];"
		"   if (head) head.appendChild(mystyle);"
		"}, true);"
	);
	return g_string_free(code, FALSE);
}

static void
adblock_destroy_db()
{
	g_string_free(blockcss, TRUE);
	blockcss = NULL;

	g_hash_table_destroy(pattern);
	pattern = NULL;
	g_hash_table_destroy(optslist);
	optslist = NULL;
	g_hash_table_destroy(urlcache);
	urlcache = NULL;
	g_hash_table_destroy(blockcssprivate);
	blockcssprivate = NULL;
	g_hash_table_destroy(navigationwhitelist);
	navigationwhitelist = NULL;
}

static void
adblock_init_db()
{
	pattern = g_hash_table_new_full(g_str_hash, g_str_equal,
				       (GDestroyNotify)g_free,
				       (GDestroyNotify)g_regex_unref);
	keys = g_hash_table_new_full(g_str_hash, g_str_equal,
				    (GDestroyNotify)g_free,
				    (GDestroyNotify)g_regex_unref);
	optslist = g_hash_table_new_full(g_str_hash, g_str_equal,
					 NULL,
					(GDestroyNotify)g_free);
	urlcache = g_hash_table_new_full(g_str_hash, g_str_equal,
					(GDestroyNotify)g_free,
					(GDestroyNotify)g_free);
	blockcssprivate = g_hash_table_new_full(g_str_hash, g_str_equal,
					       (GDestroyNotify)g_free,
					       (GDestroyNotify)g_free);
	navigationwhitelist = g_hash_table_new_full(g_direct_hash, g_str_equal,
						    NULL,
						   (GDestroyNotify)g_free);

	blockcss = g_string_new ("z-non-exist");
}

static void
adblock_download_notify_status_cb(WebKitDownload* download, GParamSpec* pspec)
{
	if (update_done)
		return;

	if (webkit_download_get_status(download) == WEBKIT_DOWNLOAD_STATUS_FINISHED) {
		GList* li = NULL;
		for (li = update_list; li != NULL; li = g_list_next(li)) {
			gchar* uri = g_strdup(webkit_download_get_destination_uri(download));
			if (g_strcmp0(li->data, uri))
				update_list = g_list_remove(update_list, li->data);
			g_free(uri);
		}
	}

	if (g_list_length(update_list) == 0) {
		adblock_reload_rules(FALSE);
		update_done = TRUE;
	}
}

static gchar*
adblock_get_filename_for_uri(const gchar* uri)
{
	gchar* filename;
	gchar* folder;
	gchar* path;

	if (uri[4] == '-' || uri[5] == '-') /* if filter is not set return */
		return NULL;

	if (!strncmp (uri, "file://", 7))
		return g_strndup(uri + 7, strlen (uri) - 7);

	folder = g_build_filename(DEFAULT_TEMP_DIR, "adblock", NULL);
	g_mkdir_with_parents(folder, 0700);

	filename = g_compute_checksum_for_string(G_CHECKSUM_MD5, uri, -1);
	path = g_build_filename(folder, filename, NULL);

	g_free(filename);
	g_free(folder);
	return path;
}

void me_setting_free(gpointer setting)
{
	MESettingStringList* strlist_setting = (MESettingStringList*)setting;
	MESettingString* string_setting = (MESettingString*)setting;

	if (string_setting->type == G_TYPE_STRING) {
		g_free(string_setting->name);
		g_free(string_setting->default_value);
		g_free(string_setting->value);
	}
	if (strlist_setting->type == G_TYPE_STRV) {
		g_free(strlist_setting->name);
		g_strfreev(strlist_setting->default_value);
		g_strfreev(strlist_setting->value);
	}
}

static void
adblock_reload_rules(gboolean custom_only)
{
	gchar* path;
	guint i = 0;

	if (pattern)
		adblock_destroy_db();
	adblock_init_db();

	if (!custom_only && *filter_list) {
		while (filter_list[i] != NULL) {
			path = adblock_get_filename_for_uri(filter_list[i]);
			if (!path) {
				i++;
				continue;
			}
			if (!adblock_file_is_up_to_date(path)) {
				WebKitNetworkRequest* request;
				WebKitDownload* download;
				gchar* destination = g_filename_to_uri(path, NULL, NULL);

				request = webkit_network_request_new(filter_list[i]);
				download = webkit_download_new(request);
				g_object_unref(request);
				webkit_download_set_destination_uri(download, destination);
				update_list = g_list_prepend(update_list, path);
				g_free(destination);
				g_signal_connect(download, "notify::status",
				G_CALLBACK(adblock_download_notify_status_cb), NULL);
				webkit_download_start(download);
			} else
				adblock_parse_file(path);
			g_free(path);
			i++;
		}
	}
	g_string_append(blockcss, " {display: none !important}\n");
}

static inline gint
adblock_check_rule(GRegex* regex,
		   const gchar* patt,
		   const gchar* req_uri,
		   const gchar* page_uri)
{
	gchar* opts;

	if (!g_regex_match_full(regex, req_uri, -1, 0, 0, NULL, NULL))
		return FALSE;

	opts = g_hash_table_lookup(optslist, patt);
	if (opts && g_regex_match_simple(",third-party", opts,
					 G_REGEX_CASELESS, G_REGEX_MATCH_NOTEMPTY)) {
		if (page_uri && g_regex_match_full(regex, page_uri, -1, 0, 0, NULL, NULL))
			return FALSE;
	}
	/* TODO: Domain opt check */
	return TRUE;
}

static inline gboolean
adblock_is_matched_by_pattern(const gchar* req_uri, const gchar* page_uri)
{
	gpointer patt, regex;
	GHashTableIter iter;

	if (USE_PATTERN_MATCHING == 0)
		return FALSE;

	g_hash_table_iter_init(&iter, pattern);
	while (g_hash_table_iter_next(&iter, &patt, &regex)) {
		if (adblock_check_rule(regex, patt, req_uri, page_uri))
		return TRUE;
	}
	return FALSE;
}

static inline gboolean
adblock_is_matched_by_key(const gchar* req_uri, const gchar* page_uri)
{
	gchar sig[SIGNATURE_SIZE + 1];
	GList* regex_bl = NULL;
	gboolean ret = FALSE;
	GString* guri;
	int pos = 0;
	gchar* uri;
	gint len;

	memset(&sig[0], 0, sizeof(sig));
	/* Signatures are made on pattern, so we need to convert url to a pattern as well */
	guri = adblock_fixup_regexp("", (gchar*)req_uri);
	uri = guri->str;
	len = guri->len;

	for (pos = len - SIGNATURE_SIZE; pos >= 0; pos--) {
		GRegex* regex;
		strncpy(sig, uri + pos, SIGNATURE_SIZE);
		regex = g_hash_table_lookup(keys, sig);

		/* Don't check if regex is already blacklisted */
		if (!regex || g_list_find(regex_bl, regex))
			continue;
		ret = adblock_check_rule(regex, sig, req_uri, page_uri);
		if (ret)
			break;
		regex_bl = g_list_prepend(regex_bl, regex);
	}
	g_string_free(guri, TRUE);
	g_list_free(regex_bl);
	return ret;
}

static gboolean
adblock_is_matched(const gchar* req_uri, const gchar* page_uri)
{
	gchar* value;

	if ((value = g_hash_table_lookup(urlcache, req_uri))) {
		if (value[0] == '0')
			return FALSE;
		else
			return TRUE;
	}

	if (adblock_is_matched_by_key(req_uri, page_uri)) {
		g_hash_table_insert(urlcache, g_strdup(req_uri), g_strdup("1"));
		return TRUE;
	}

	if (adblock_is_matched_by_pattern(req_uri, page_uri)) {
		g_hash_table_insert(urlcache, g_strdup(req_uri), g_strdup("1"));
		return TRUE;
	}
	g_hash_table_insert(urlcache, g_strdup(req_uri), g_strdup("0"));
	return FALSE;
}

static gchar*
adblock_prepare_urihider_js(GList* uris)
{
	GString* js = g_string_new (
		"(function() {"
		"function getElementsByAttribute(strTagName, strAttributeName, arrAttributeValue) {"
		"	var arrElements = document.getElementsByTagName(strTagName);"
		"	var arrReturnElements = new Array();"
		"	for (var j = 0; j < arrAttributeValue.length; j++) {"
		"		var strAttributeValue = arrAttributeValue[j];"
		"		for (var i = 0; i < arrElements.length; i++) {"
		"			var oCurrent = arrElements[i];"
		"			var oAttribute = oCurrent.getAttribute && oCurrent.getAttribute(strAttributeName);"
		"			if (oAttribute && oAttribute.length > 0 && strAttributeValue.indexOf(oAttribute) != -1)"
		"				arrReturnElements.push(oCurrent);"
		"		}"
		"	}"
		"	return arrReturnElements;"
		"};"
		"function hideElementBySrc(uris) {"
		"	var oElements = getElementsByAttribute('img', 'src', uris);"
		"	if (oElements.length == 0)"
		"		oElements = getElementsByAttribute('iframe', 'src', uris);"
		"	for (var i = 0; i < oElements.length; i++) {"
		"		oElements[i].style.visibility = 'hidden !important';"
		"		oElements[i].style.width = '0';"
		"		oElements[i].style.height = '0';"
		"	}"
		"};"
		"var uris=new Array();"
	);
	GList* li = NULL;

	for (li = uris; li != NULL; li = g_list_next(li))
		g_string_append_printf(js, "uris.push('%s');", (gchar*)li->data);

	g_string_append(js, "hideElementBySrc(uris);})();");

	return g_string_free(js, FALSE);
}

static gboolean
adblock_navigation_policy_decision_requested_cb(WebKitWebView*             web_view,
						WebKitWebFrame*            web_frame,
						WebKitNetworkRequest*      request,
						WebKitWebNavigationAction* action,
						WebKitWebPolicyDecision*   decision)
{
	const gchar* uri = webkit_network_request_get_uri(request);
	if (g_str_has_prefix(uri, "abp:")) {
		gchar** parts;
		gchar* filter;
		if (g_str_has_prefix(uri, "abp:subscribe?location="))
			uri = &uri[23];
		else if (g_str_has_prefix(uri, "abp://subscribe?location="))
			uri = &uri[25];
		else
			return FALSE;

		parts = g_strsplit(uri, "&", 2);
		filter = soup_uri_decode(parts[0]);
		webkit_web_policy_decision_ignore(decision);
		g_free(filter);
		g_strfreev(parts);
		return TRUE;
	}

	if (web_frame == webkit_web_view_get_main_frame(web_view)) {
		const gchar* req_uri = webkit_network_request_get_uri(request);
		g_hash_table_replace(navigationwhitelist, web_view, g_strdup(req_uri));
	}
	return false;
}

gboolean uri_is_http(const gchar* uri)
{
	if (!uri)
		return FALSE;

	if (g_str_has_prefix(uri, "http://") ||
	    g_str_has_prefix(uri, "https://"))
		return TRUE;

	return FALSE;
}

static gboolean uri_is_blank(const gchar* uri)
{
	return !(uri && (uri[0] != 0) && !g_str_has_prefix (uri, "about:"));
}

static void
adblock_resource_request_starting_cb(WebKitWebView*         web_view,
				     WebKitWebFrame*        web_frame,
				     WebKitWebResource*     web_resource,
				     WebKitNetworkRequest*  request,
				     WebKitNetworkResponse* response,
				     void *view)
{
	gboolean is_favicon = FALSE;
	const gchar* req_uri;
	const char *page_uri;
	GList* blocked_uris;
	const gchar* path;
	GURI *guri;

	page_uri = webkit_web_view_get_uri(web_view);
	/* Skip checks on about: pages */
	if (uri_is_blank(page_uri))
		return;
	req_uri = webkit_network_request_get_uri(request);

	if (!g_strcmp0(req_uri, g_hash_table_lookup(navigationwhitelist, web_view)))
		return;

	if (!uri_is_http(req_uri))
		return;
	guri = g_uri_new(req_uri);

	if (!guri)
		return;
	path = g_uri_get_path(guri);

	if (path)
		is_favicon = g_str_has_suffix(path, "favicon.ico");
	g_object_unref(guri);

	if (is_favicon)
		return;

	if (response != NULL) /* request is caused by redirect */ {
		if (web_frame == webkit_web_view_get_main_frame(web_view)) {
			g_hash_table_replace(navigationwhitelist, web_view, g_strdup(req_uri));
			return;
		}
	}

	if (adblock_is_matched(req_uri, page_uri)) {
		blocked_uris = g_object_get_data(G_OBJECT(web_view), "blocked-uris");
		blocked_uris = g_list_prepend(blocked_uris, g_strdup(req_uri));
		webkit_network_request_set_uri(request, "about:blank");
		g_object_set_data(G_OBJECT(web_view), "blocked-uris", blocked_uris);
	}
}

static void
adblock_populate_popup_cb(WebKitWebView* web_view, GtkWidget* menu)
{
	WebKitHitTestResultContext context;
	WebKitHitTestResult* hit_test;
	GdkEventButton event;
	GtkWidget* menuitem;
	gchar *uri;
	gint x, y;
#if GTK_CHECK_VERSION(3, 0, 0)
	GdkDevice *pointer;
	GdkWindow *win = gtk_widget_get_window(GTK_WIDGET(web_view));

#if GTK_CHECK_VERSION(3, 20, 0)
	GdkSeat *seat;

	seat = gdk_display_get_default_seat(gdk_window_get_display(win));
	pointer = gdk_seat_get_pointer(seat);
#else
	GdkDeviceManager *device_manager;

	device_manager = gdk_display_get_device_manager(gdk_window_get_display(win));
	pointer = gdk_device_manager_get_client_pointer(device_manager);
#endif
	gdk_window_get_device_position(win, pointer, &x, &y, NULL);
#else
	gdk_window_get_pointer(gtk_widget_get_window(GTK_WIDGET(web_view)), &x, &y, NULL);
#endif
	event.x = x;
	event.y = y;
	hit_test = webkit_web_view_get_hit_test_result(web_view, &event);

	if (!hit_test)
		return;
	g_object_get(hit_test, "context", &context, NULL);

	if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_IMAGE) {
		g_object_get(hit_test, "image-iri", &uri, NULL);
		menuitem = gtk_menu_item_new_with_mnemonic("Bl_ock image");
	} else if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK) {
		g_object_get(hit_test, "image-iri", &uri, NULL);
		menuitem = gtk_menu_item_new_with_mnemonic("Bl_ock link");
	} else
		return;
	gtk_widget_show(menuitem);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	g_object_set_data_full(G_OBJECT(menuitem), "uri", uri, (GDestroyNotify)g_free);
}

static void
adblock_load_finished_cb(WebKitWebView  *web_view,
			 WebKitWebFrame *web_frame,
			 gpointer        user_data)
{
	GList* uris = g_object_get_data(G_OBJECT(web_view), "blocked-uris");
	gchar* script;
	GList* li;

	if (g_list_nth_data(uris, 0) == NULL)
		return;

	script = adblock_prepare_urihider_js(uris);
	webkit_web_view_execute_script(web_view, script);
	for (li = uris; li != NULL; li = g_list_next(li))
		uris = g_list_remove(uris, li->data);
	g_free(script);
	g_object_set_data(G_OBJECT(web_view), "blocked-uris", uris);
}


/** \todo reevaluate function*/
static gchar*
js_string_utf8(JSStringRef js_string)
{
	gchar* string_utf8;
	size_t size_utf8;

	g_return_val_if_fail(js_string, NULL);

	size_utf8 = JSStringGetMaximumUTF8CStringSize(js_string);
	string_utf8 = g_new(gchar, size_utf8);
	JSStringGetUTF8CString(js_string, string_utf8, size_utf8);
	return string_utf8;
}

/** \todo reevaluate function*/
gchar*
js_script_eval(JSContextRef js_context,
		      const gchar* script,
		      gchar**      exception)
{
	JSGlobalContextRef temporary_context = NULL;
	JSValueRef js_exception = NULL;
	JSStringRef js_value_string;
	JSStringRef js_script;
	JSValueRef js_value;
	gchar* value;

	g_return_val_if_fail(script, FALSE);

	if (!js_context) {
		temporary_context = JSGlobalContextCreateInGroup(NULL, NULL);
		js_context = temporary_context;
	}

	js_script = JSStringCreateWithUTF8CString(script);
	js_value = JSEvaluateScript(js_context, js_script,
	JSContextGetGlobalObject(js_context), NULL, 0, &js_exception);
	JSStringRelease(js_script);

	if (!js_value) {
		JSStringRef js_message = JSValueToStringCopy(js_context,
							     js_exception,
							     NULL);
		g_return_val_if_fail(js_message != NULL, NULL);

		value = js_string_utf8(js_message);
		if (exception)
			*exception = value;
		else {
			g_warning("%s", value);
			g_free(value);
		}
		JSStringRelease(js_message);
		if (temporary_context)
			JSGlobalContextRelease(temporary_context);
		return NULL;
	}

	js_value_string = JSValueToStringCopy(js_context, js_value, NULL);
	value = js_string_utf8(js_value_string);
	JSStringRelease(js_value_string);
	if (temporary_context)
		JSGlobalContextRelease(temporary_context);
	return value;
}

static void
adblock_window_object_cleared_cb(WebKitWebView*  web_view,
				 WebKitWebFrame* web_frame,
				 JSContextRef    js_context,
				 JSObjectRef     js_window)
{
	const char *page_uri;
	gchar* script;

	page_uri = webkit_web_frame_get_uri(web_frame);
	/* Don't add adblock css into speeddial and about: pages */
	if (!uri_is_http(page_uri))
		return;

	script = adblock_build_js(page_uri);
	if (!script)
		return;

	g_free(js_script_eval(js_context, script, NULL));
	g_free(script);
}

static GString*
adblock_fixup_regexp(const gchar* prefix,
		     gchar*       src)
{
	GString* str;
	int len = 0;

	if (!src)
		return NULL;

	str = g_string_new(prefix);

	/* lets strip first .* */
	if (src[0] == '*') {
		src++;
	}

	do {
		switch (*src) {
		case '*':
			g_string_append(str, ".*");
			break;
		/*case '.':
			g_string_append(str, "\\.");
			break;*/
		case '?':
			g_string_append(str, "\\?");
			break;
		case '|':
		/* FIXME: We actually need to match :[0-9]+ or '/'. Sign means
		"here could be port number or nothing". So bla.com^ will match
		bla.com/ or bla.com:8080/ but not bla.com.au/ */
		case '^':
		case '+':
			break;
		default:
			g_string_append_printf(str,"%c", *src);
			break;
		}
		src++;
	} while (*src);

	len = str->len;
	/* We don't need .* in the end of a url. That's stupid */
	if (str->str && str->str[len-1] == '*' && str->str[len-2] == '.')
		g_string_erase(str, len-2, 2);

	return str;
}

static gboolean
adblock_compile_regexp(GString* gpatt, gchar* opts)
{
	int signature_count = 0;
	GError* error = NULL;
	GRegex* regex;
	int pos = 0;
	gchar *patt;
	gchar *sig;
	int len;

	if (!gpatt)
		return FALSE;

	patt = gpatt->str;
	len = gpatt->len;

	/* TODO: Play with optimization flags */
	regex = g_regex_new(patt, G_REGEX_OPTIMIZE,
			    G_REGEX_MATCH_NOTEMPTY, &error);
	if (error) {
		g_warning("%s: %s", G_STRFUNC, error->message);
		g_error_free(error);
		return TRUE;
	}

	if (g_regex_match_simple("^/.*[\\^\\$\\*].*/$", patt, G_REGEX_UNGREEDY, G_REGEX_MATCH_NOTEMPTY)) {
		/* Pattern is a regexp chars */
		g_hash_table_insert(pattern, patt, regex);
		g_hash_table_insert(optslist, patt, g_strdup(opts));
		return FALSE;
	}

	for (pos = len - SIGNATURE_SIZE; pos >= 0; pos--) {
		sig = g_strndup(patt + pos, SIGNATURE_SIZE);
		if (!g_regex_match_simple("[\\*]", sig, G_REGEX_UNGREEDY, G_REGEX_MATCH_NOTEMPTY) &&
		    !g_hash_table_lookup(keys, sig)) {
			g_hash_table_insert(keys, sig, regex);
			g_hash_table_insert(optslist, sig, g_strdup(opts));
			signature_count++;
		} else {
			if (g_regex_match_simple("^\\*", sig, G_REGEX_UNGREEDY, G_REGEX_MATCH_NOTEMPTY) &&
			    !g_hash_table_lookup(pattern, patt)) {
				g_hash_table_insert(pattern, patt, regex);
				g_hash_table_insert(optslist, patt, g_strdup(opts));
			}
			g_free(sig);
		}
	}

	if (signature_count > 1 && g_hash_table_lookup(pattern, patt)) {
		g_hash_table_steal(pattern, patt);
		return TRUE;
	}

	return FALSE;
}

static inline gchar*
adblock_add_url_pattern(gchar* prefix, gchar* type, gchar* line)
{
	GString* format_patt;
	gboolean should_free;
	gchar** data;
	gchar* patt;
	gchar* opts;

	data = g_strsplit(line, "$", -1);
	if (!data || !data[0]) {
		g_strfreev(data);
		return NULL;
	}

	if (data[1] && data[2]) {
		patt = g_strconcat(data[0], data[1], NULL);
		opts = g_strconcat(type, ",", data[2], NULL);
	} else if (data[1]) {
		patt = data[0];
		opts = g_strconcat(type, ",", data[1], NULL);
	} else {
		patt = data[0];
		opts = type;
	}

	if (g_regex_match_simple("subdocument", opts,
				  G_REGEX_CASELESS, G_REGEX_MATCH_NOTEMPTY)) {
		if (data[1] && data[2])
			g_free(patt);
		if (data[1])
			g_free(opts);
		g_strfreev(data);
		return NULL;
	}

	format_patt = adblock_fixup_regexp(prefix, patt);

	should_free = adblock_compile_regexp(format_patt, opts);

	if (data[1] && data[2])
		g_free(patt);
	if (data[1])
		g_free(opts);
	g_strfreev(data);

	return g_string_free(format_patt, should_free);
}

static inline void
adblock_frame_add(gchar* line)
{
	const gchar* separator = " , ";

	line++;
	line++;

	if (strchr(line, '\'') ||
	   (strchr(line, ':') && !g_regex_match_simple(".*\\[.*:.*\\].*",
	    line, G_REGEX_CASELESS, G_REGEX_MATCH_NOTEMPTY)))
		return;

	g_string_append(blockcss, separator);
	g_string_append(blockcss, line);
}

static void
adblock_update_css_hash (gchar* domain, gchar* value)
{
	const gchar* olddata;
	gchar* newdata;

	if ((olddata = g_hash_table_lookup(blockcssprivate, domain))) {
		newdata = g_strconcat(olddata, " , ", value, NULL);
		g_hash_table_replace(blockcssprivate, g_strdup(domain), newdata);
	} else
		g_hash_table_insert(blockcssprivate, g_strdup(domain), g_strdup(value));
}

static inline void
adblock_frame_add_private (const gchar* line, const gchar* sep)
{
	gchar** data;

	data = g_strsplit(line, sep, 2);
	if (!(data[1] && *data[1])
	    ||  strchr(data[1], '\'')
	    || (strchr(data[1], ':')
	    && !g_regex_match_simple(".*\\[.*:.*\\].*", data[1],
				     G_REGEX_CASELESS, G_REGEX_MATCH_NOTEMPTY))) {
		g_strfreev(data);
		return;
	}

	if (strchr(data[0], ',')) {
		gchar** domains;
		gint i;

		domains = g_strsplit(data[0], ",", -1);
		for (i = 0; domains[i]; i++) {
			gchar* domain;

			domain = domains[i];
			/* Ignore Firefox-specific option */
			if (!g_strcmp0(domain, "~pregecko2"))
				continue;
			/* FIXME: ~ should negate match
			 * Replaced the original behaviour of skipping the '~'
			 * and using this rule to skipping the rule.
			 */
			if (domain[0] == '~')
				continue; /* skipped original code: domain++; */
			adblock_update_css_hash(g_strstrip(domain), data[1]);
		}
		g_strfreev(domains);
	} else {
		adblock_update_css_hash(data[0], data[1]);
	}
	g_strfreev(data);
}

static gchar*
adblock_parse_line(gchar* line)
{
	/* Skip invalid, empty and comment lines */
	if (!(line && line[0] != ' ' && line[0] != '!' && line[0]))
		return NULL;

	/* FIXME: No support for whitelisting */
	if (line[0] == '@' && line[1] == '@')
		return NULL;
	/* FIXME: No support for [include] and [exclude] tags */
	if (line[0] == '[')
		return NULL;

	g_strchomp(line);

	/* Got CSS block hider */
	if (line[0] == '#' && line[1] == '#' ) {
		adblock_frame_add(line);
		return NULL;
	}
	/* Got CSS block hider. Workaround */
	if (line[0] == '#')
		return NULL;

	/* Got per domain CSS hider rule */
	if (strstr(line, "##")) {
		adblock_frame_add_private(line, "##");
		return NULL;
	}
	/* Got per domain CSS hider rule. Workaround */
	if (strchr(line, '#')) {
		adblock_frame_add_private(line, "#");
		return NULL;
	}

	/* Got URL blocker rule */
	if (line[0] == '|' && line[1] == '|' ) {
		line++;
		line++;
		return adblock_add_url_pattern("", "fulluri", line);
	}
	if (line[0] == '|') {
		line++;
		return adblock_add_url_pattern("^", "fulluri", line);
	}
	return adblock_add_url_pattern("", "uri", line);
}

static GDateMonth
str_month_name_to_gdate(const gchar* month)
{
	guint i;
	const gchar* months[] = {
		"", "January", "February", "March", "April", "May", "June",
		"July", "August", "September", "October", "November", "December"
	};

	for (i = 0; i < G_N_ELEMENTS(months); i++) {
		if (strncmp(month, months[i], 3) == 0)
			return i;
	}
	return 0;
}

static gboolean
adblock_file_is_up_to_date(gchar* path)
{
	gboolean found_meta = FALSE;
	gint days_to_expire = 0;
	gchar* timestamp = NULL;
	gint days_elapsed = 0;
	gint fs_days_elapsed;
	gchar line[2000];
	gint least_days;
	FILE* file;
	guint i;

	/* Check a chunk of header for update info */
	file = g_fopen(path, "r");
	if (!file)
		return FALSE;

	for (i = 0; i <= 15; i++) {
		if (!fgets(line, sizeof(line), file))
			continue;

		if (strncmp("! Expires", line, 9) == 0) {
			gchar** parts = g_strsplit(line, " ", 4);
			days_to_expire = atoi(parts[2]);
			g_strfreev(parts);
			found_meta = TRUE;
		}

		if (strncmp("! This list expires after", line, 25) == 0) {
			gchar** parts = g_strsplit(line, " ", 7);

			if (strncmp(parts[6], "days", 4) == 0)
				days_to_expire = atoi(parts[5]);
			if (strncmp(parts[6], "hours", 5) == 0)
				days_to_expire = (atoi(parts[5])) / 24;

			g_strfreev(parts);
			found_meta = TRUE;
		}

		if ((strncmp("! Last mod", line, 10) == 0) ||
		    (strncmp("! Updated", line, 9) == 0)) {
			gchar** parts = g_strsplit(line, ":", 2);
			timestamp = g_strdup(parts[1] + 1);
			g_strchomp(timestamp);
			g_strfreev(parts);
			found_meta = TRUE;
		}
	}

	fclose(file);

	if (!found_meta) {
		g_print("Adblock: no metadata found in %s (broken "
			"download?)\n", path);
		return FALSE;
	}

	/* query filesystem about file change, maybe there is no update yet
	 * or there is no "modified" metadata to check, otherwise we will
	 * repeatedly download files that have no new updates */
	{
		GFile* filter_file = g_file_new_for_path(path);
		GDate* fs_mod_date = g_date_new();
		GDate* current = g_date_new();
		GTimeVal mod_time;
		GFileInfo* info;

		info = g_file_query_info(filter_file, "time:modified", 0,
					 NULL, NULL);

		g_file_info_get_modification_time(info, &mod_time);
		g_date_set_time_t (current, time(NULL));
		g_date_set_time_val(fs_mod_date, &mod_time);

		fs_days_elapsed = g_date_days_between(fs_mod_date, current);

		g_date_free(current);
		g_date_free(fs_mod_date);
	}

	/* If there is no update metadata but file is valid, assume one week */
	if ((!days_to_expire && !timestamp) && fs_days_elapsed < 7)
		return TRUE;

	if (days_to_expire && timestamp != NULL) {
		GDate* mod_date = g_date_new();
		GDate* current = g_date_new();
		gboolean use_dots = FALSE;
		gchar** parts;

		/* Common dates are 20 Mar 2012, 20.08.2012 */
		if (strrchr(timestamp, '.')) {
			use_dots = TRUE;
			/* In case of date like '20.08.2012 12:34'
			* we should also nuke the time part */
			if (strrchr(timestamp, ' ')) {
				gchar** part = g_strsplit(timestamp, " ", 2);
				parts = g_strsplit(part[0], ".", 4);
				g_strfreev(part);
			} else
				parts = g_strsplit(timestamp, ".", 4);
		} else
			parts = g_strsplit(timestamp, " ", 4);

		if (use_dots)
			g_date_set_month(mod_date, atoi(parts[1]));
		else
			g_date_set_month(mod_date, str_month_name_to_gdate(parts[1]));

		/* check if first part is year 201(2) or day */
		if (strncmp(parts[0], "201", 3) == 0) {
			g_date_set_day(mod_date, atoi(parts[2]));
			g_date_set_year(mod_date, atoi(parts[0]));
		} else {
			g_date_set_day(mod_date, atoi(parts[0]));
			g_date_set_year(mod_date, atoi(parts[2]));
		}
		g_strfreev(parts);

		g_date_set_time_t(current, time(NULL));
		days_elapsed = g_date_days_between(mod_date, current);

		g_date_free(current);
		g_date_free(mod_date);
		g_free(timestamp);
	}

	/* File from the future? Assume up to date */
	if (days_elapsed < 0) {
		g_print("Adblock: file %s appears to be from the future,"
			"check your system clock!\n", path);
		return TRUE;
	}

	least_days = days_elapsed < fs_days_elapsed ? days_elapsed : fs_days_elapsed;

	return (least_days < days_to_expire);
}

static void
adblock_parse_file(gchar* path)
{
	gchar line[2000];
	FILE* file;

	if ((file = g_fopen(path, "r"))) {
		while (fgets(line, sizeof(line), file)) {
			adblock_parse_line(line);
		}
		fclose(file);
	}
}

static void
adblock_deactivate_tabs(WebKitWebView* web_view)
{
	g_signal_handlers_disconnect_by_func(
		web_view, adblock_window_object_cleared_cb, 0);
	g_signal_handlers_disconnect_by_func(
		web_view, adblock_populate_popup_cb, NULL);
	g_signal_handlers_disconnect_by_func(
		web_view, adblock_resource_request_starting_cb, NULL);
	g_signal_handlers_disconnect_by_func(
		web_view, adblock_load_finished_cb, NULL);
	g_signal_handlers_disconnect_by_func(
		web_view, adblock_navigation_policy_decision_requested_cb, NULL);
}

void
adblock_add_tab_cb(WebKitWebView* web_view)
{
	g_signal_connect(web_view, "window-object-cleared",
		G_CALLBACK(adblock_window_object_cleared_cb), 0);
	g_signal_connect_after(web_view, "populate-popup",
		G_CALLBACK(adblock_populate_popup_cb), NULL);
	g_signal_connect(web_view, "navigation-policy-decision-requested",
		G_CALLBACK(adblock_navigation_policy_decision_requested_cb), NULL);
	g_signal_connect(web_view, "resource-request-starting",
		G_CALLBACK(adblock_resource_request_starting_cb), NULL);
	g_signal_connect(web_view, "load-finished",
		G_CALLBACK(adblock_load_finished_cb), NULL);
}

void
adblock_remove_tab_cb(WebKitWebView* web_view)
{
	g_hash_table_remove(navigationwhitelist, web_view);
}

void
adblock_deactivate_cb(WebKitBrowser* browser)
{
	GtkNotebook *notebook;
	gint i = 0;
	g_signal_handlers_disconnect_by_func(browser,
					     adblock_deactivate_cb, NULL);
	g_signal_handlers_disconnect_by_func(browser,
					     adblock_add_tab_cb, NULL);
	g_signal_handlers_disconnect_by_func(browser,
					     adblock_remove_tab_cb, NULL);

	notebook = webkit_browser_get_tabs(browser);
	for (i = 0; i < gtk_notebook_get_n_pages(notebook); i++)
		adblock_deactivate_tabs(WEBKIT_WEB_VIEW(gtk_notebook_get_nth_page(notebook, i)));

	adblock_destroy_db();
}

void
adblock_activate_cb(WebKitBrowser* browser)
{
	adblock_reload_rules(FALSE);
}
