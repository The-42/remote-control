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
#include <string.h>
#include <ifaddrs.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <sys/utsname.h>
#include <sys/ioctl.h>

#include "javascript.h"

#define SYSINFO_RELEASE_FILE "/etc/os-release"

#define SYSINFO_RELEASE_GROUP "INFO"
#define SYSINFO_RELEASE_GROUP_DATA "["SYSINFO_RELEASE_GROUP"]\n"

struct sysinfo {
	struct remote_control_data *rcd;
};

static struct ifaddrs *sysinfo_get_if(struct ifaddrs *ifaddr, char *interface)
{
	struct ifaddrs *ifa;

	for (ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
		if (!strncmp(ifa->ifa_name, "lo", IFNAMSIZ))
			continue;
		if (interface && strncmp(ifa->ifa_name, interface, IFNAMSIZ))
			continue;
		if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
			continue;
		return ifa;
	}
	return NULL;
}

static char *sysinfo_get_release_info(JSContextRef context, const char *type,
	JSValueRef *exception)
{
	goffset data_len, group_data_len;
	GFileInputStream *read = NULL;
	GKeyFile *keyfile = NULL;
	GFileInfo *info = NULL;
	GFile *file = NULL;
	char *buff = NULL;
	char *ret = NULL;

	file = g_file_new_for_path(SYSINFO_RELEASE_FILE);
	if (!file) {
		javascript_set_exception_text(context, exception,
				"Failed to open file %s", SYSINFO_RELEASE_FILE);
		goto cleanup;
	}
	read = g_file_read(file, NULL, NULL);
	if (!read) {
		javascript_set_exception_text(context, exception,
				"Failed to get read interface for file %s",
				SYSINFO_RELEASE_FILE);
		goto cleanup;
	}
	info = g_file_input_stream_query_info(read,
			G_FILE_ATTRIBUTE_STANDARD_SIZE, NULL, NULL);
	if (!info) {
		javascript_set_exception_text(context, exception,
				"Failed to get information for file %s",
				SYSINFO_RELEASE_FILE);
		goto cleanup;
	}
	keyfile = g_key_file_new();
	if (!keyfile) {
		javascript_set_exception_text(context, exception,
				"Failed to create object");
		goto cleanup;
	}
	if (!g_file_info_has_attribute(info, G_FILE_ATTRIBUTE_STANDARD_SIZE)) {
		javascript_set_exception_text(context, exception,
				"Unable to detect the file size for file %s",
				SYSINFO_RELEASE_FILE);
		goto cleanup;
	}
	group_data_len = strlen(SYSINFO_RELEASE_GROUP_DATA);
	data_len = g_file_info_get_size(info) + group_data_len;
	buff = g_new(char, data_len);
	if (!buff) {
		javascript_set_exception_text(context, exception,
				"Failed to alloc buffer");
		goto cleanup;
	}
	strcpy(buff, SYSINFO_RELEASE_GROUP_DATA);
	if (!g_input_stream_read_all(G_INPUT_STREAM(read),
			&buff[group_data_len], data_len - group_data_len, NULL,
			NULL, NULL)) {
		javascript_set_exception_text(context, exception,
				"Failed to read from file %s",
				SYSINFO_RELEASE_FILE);
		goto cleanup;
	}
	if (!g_key_file_load_from_data(keyfile, buff, data_len, G_KEY_FILE_NONE,
			NULL)) {
		javascript_set_exception_text(context, exception,
				"Failed to load from data");
		goto cleanup;
	}
	ret = g_key_file_get_value(keyfile, SYSINFO_RELEASE_GROUP, type, NULL);

cleanup:
	if (buff)
		g_free(buff);
	if (info)
		g_object_unref(info);
	if (read)
		g_object_unref(read);
	if (file)
		g_object_unref(file);
	if (keyfile)
		g_key_file_free(keyfile);
	return ret;
}

static JSValueRef sysinfo_function_local_ip(
	JSContextRef context, JSObjectRef function, JSObjectRef object,
	size_t argc, const JSValueRef argv[], JSValueRef *exception)
{
	struct sysinfo *inf = JSObjectGetPrivate(object);
	struct ifaddrs *ifaddr = NULL, *ifa;
	gchar ip[INET_ADDRSTRLEN];
	struct sockaddr_in *sin;
	char *interface = NULL;
	JSValueRef ret = NULL;

	if (!inf) {
		javascript_set_exception_text(context, exception,
				JS_ERR_INVALID_OBJECT_TEXT);
		goto cleanup;
	}
	/* Usage: localIP([interface]) */
	switch (argc) {
	case 1:
		interface = javascript_get_string(context, argv[0], exception);
		if (!interface)
			goto cleanup;
		break;
	case 0:
		break;
	default:
		javascript_set_exception_text(context, exception,
				JS_ERR_INVALID_ARG_COUNT);
		goto cleanup;
	}

	if (-1 == getifaddrs(&ifaddr)) {
		javascript_set_exception_text(context, exception,
			"Failed to get if addresses: %s", g_strerror(errno));
		goto cleanup;
	}
	ifa = sysinfo_get_if(ifaddr, interface);
	if (!ifa) {
		javascript_set_exception_text(context, exception,
			"Failed to get if address");
		goto cleanup;
	}
	sin = (struct sockaddr_in *)ifa->ifa_addr;
	if (!inet_ntop(AF_INET, &sin->sin_addr, ip, sizeof(ip))) {
		javascript_set_exception_text(context, exception,
			"Failed to convert if address");
		goto cleanup;
	}
	ret = javascript_make_string(context, ip, exception);

cleanup:
	if (ifaddr)
		freeifaddrs(ifaddr);
	if (interface)
		g_free(interface);

	return ret;
}

static JSValueRef sysinfo_function_local_mac_address(
	JSContextRef context, JSObjectRef function, JSObjectRef object,
	size_t argc, const JSValueRef argv[], JSValueRef *exception)
{
	struct sysinfo *inf = JSObjectGetPrivate(object);
	struct ifaddrs *ifaddr = NULL, *ifa;
	const unsigned char* mac;
	char *interface = NULL;
	JSValueRef ret = NULL;
	struct ifreq ifr;
	int fd = -1;

	if (!inf) {
		javascript_set_exception_text(context, exception,
				JS_ERR_INVALID_OBJECT_TEXT);
		goto cleanup;
	}
	/* Usage: localMacAddress([interface]) */
	switch (argc) {
	case 1:
		interface = javascript_get_string(context, argv[0], exception);
		if (!interface)
			goto cleanup;
		break;
	case 0:
		break;
	default:
		javascript_set_exception_text(context, exception,
				JS_ERR_INVALID_ARG_COUNT);
		goto cleanup;
	}

	if (-1 == getifaddrs(&ifaddr)) {
		javascript_set_exception_text(context, exception,
			"Failed to get if addresses: %s", g_strerror(errno));
		goto cleanup;
	}
	ifa = sysinfo_get_if(ifaddr, interface);
	if (!ifa) {
		javascript_set_exception_text(context, exception,
			"Failed to get if address");
		goto cleanup;
	}
	if (strlen(ifa->ifa_name) >= sizeof(ifr.ifr_name)) {
		javascript_set_exception_text(context, exception,
			"Interface name to long: %s (max %d)", ifa->ifa_name,
			sizeof(ifr.ifr_name));
		goto cleanup;
	}
	strcpy(ifr.ifr_name, ifa->ifa_name);

	fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (fd == -1) {
		javascript_set_exception_text(context, exception,
			"Failed to open socket %d", errno);
		goto cleanup;
	}
	if (ioctl(fd, SIOCGIFHWADDR, &ifr)==-1) {
		javascript_set_exception_text(context, exception,
			"Failed to invoke ioctl %d", errno);
		goto cleanup;
	}
	if (ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER) {
		javascript_set_exception_text(context, exception,
			"Not an ethernet interface");
		goto cleanup;
	}
	mac = (unsigned char*)ifr.ifr_hwaddr.sa_data;
	ret = javascript_sprintf(context, exception,
			"%02X:%02X:%02X:%02X:%02X:%02X",
			mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

cleanup:
	if (fd != -1)
		close(fd);
	if (ifaddr)
		freeifaddrs(ifaddr);
	if (interface)
		g_free(interface);

	return ret;
}

static JSValueRef sysinfo_function_local_host_name(
	JSContextRef context, JSObjectRef function, JSObjectRef object,
	size_t argc, const JSValueRef argv[], JSValueRef *exception)
{
	struct sysinfo *inf = JSObjectGetPrivate(object);
	JSValueRef ret = NULL;
	struct utsname uts;

	if (!inf) {
		javascript_set_exception_text(context, exception,
				JS_ERR_INVALID_OBJECT_TEXT);
		goto cleanup;
	}
	/* Usage: localHostName() */
	if (argc) {
		javascript_set_exception_text(context, exception,
				JS_ERR_INVALID_ARG_COUNT);
		goto cleanup;
	}

	if (uname(&uts)) {
		javascript_set_exception_text(context, exception,
				"Failed to get information");
		goto cleanup;
	}
	ret = javascript_make_string(context, uts.nodename, exception);

cleanup:
	return ret;
}

static JSValueRef sysinfo_function_remote_uri(
	JSContextRef context, JSObjectRef function, JSObjectRef object,
	size_t argc, const JSValueRef argv[], JSValueRef *exception)
{
	struct sysinfo *inf = JSObjectGetPrivate(object);
	JSValueRef ret = NULL;
	gchar *uri = NULL;

	if (!inf || !inf->rcd) {
		javascript_set_exception_text(context, exception,
				JS_ERR_INVALID_OBJECT_TEXT);
		goto cleanup;
	}
	/* Usage: remoteUri() */
	if (argc) {
		javascript_set_exception_text(context, exception,
				JS_ERR_INVALID_ARG_COUNT);
		goto cleanup;
	}

	uri = g_key_file_get_value(inf->rcd->config, "browser", "uri", NULL);
	if (uri)
		ret = javascript_make_string(context, uri, exception);

cleanup:
	if (uri)
		g_free(uri);
	return ret;
}

static JSValueRef sysinfo_function_release_version(
	JSContextRef context, JSObjectRef function, JSObjectRef object,
	size_t argc, const JSValueRef argv[], JSValueRef *exception)
{
	struct sysinfo *inf = JSObjectGetPrivate(object);
	JSValueRef ret = NULL;
	char *version = NULL;

	if (!inf) {
		javascript_set_exception_text(context, exception,
				JS_ERR_INVALID_OBJECT_TEXT);
		goto cleanup;
	}
	/* Usage: releaseVersion() */
	if (argc) {
		javascript_set_exception_text(context, exception,
				JS_ERR_INVALID_ARG_COUNT);
		goto cleanup;
	}

	version = sysinfo_get_release_info(context, "VERSION", exception);
	if (!version)
		goto cleanup;
	ret = javascript_make_string(context, version, exception);

cleanup:
	if (version)
		g_free(version);
	return ret;
}

static JSValueRef sysinfo_function_release_date(
	JSContextRef context, JSObjectRef function, JSObjectRef object,
	size_t argc, const JSValueRef argv[], JSValueRef *exception)
{
	struct sysinfo *inf = JSObjectGetPrivate(object);
	JSValueRef ret = NULL;
	char *date = NULL;

	if (!inf) {
		javascript_set_exception_text(context, exception,
				JS_ERR_INVALID_OBJECT_TEXT);
		goto cleanup;
	}
	/* Usage: releaseDate() */
	if (argc) {
		javascript_set_exception_text(context, exception,
				JS_ERR_INVALID_ARG_COUNT);
		goto cleanup;
	}

	date = sysinfo_get_release_info(context, "DATE", exception);
	if (!date)
		goto cleanup;
	ret = javascript_make_string(context, date, exception);

cleanup:
	if (date)
		g_free(date);
	return ret;
}

static struct sysinfo *sysinfo_new(JSContextRef context,
	struct javascript_userdata *data)
{
	struct sysinfo *inf = g_new0(struct sysinfo, 1);

	if (!inf) {
		g_warning("js-request: failed to allocate memory");
		return NULL;
	}
	inf->rcd = data->rcd;

	return inf;
}

static void sysinfo_finalize(JSObjectRef object)
{
	struct sysinfo *inf = JSObjectGetPrivate(object);

	if (!inf)
		return;

	g_free(inf);
}

static const JSStaticFunction sysinfo_functions[] = {
	{
		.name = "localIP",
		.callAsFunction = sysinfo_function_local_ip,
		.attributes = kJSPropertyAttributeDontDelete,
	},{
		.name = "localMacAddress",
		.callAsFunction = sysinfo_function_local_mac_address,
		.attributes = kJSPropertyAttributeDontDelete,
	},{
		.name = "localHostName",
		.callAsFunction = sysinfo_function_local_host_name,
		.attributes = kJSPropertyAttributeDontDelete,
	},{
		.name = "remoteUri",
		.callAsFunction = sysinfo_function_remote_uri,
		.attributes = kJSPropertyAttributeDontDelete,
	},{
		.name = "releaseVersion",
		.callAsFunction = sysinfo_function_release_version,
		.attributes = kJSPropertyAttributeDontDelete,
	},{
		.name = "releaseDate",
		.callAsFunction = sysinfo_function_release_date,
		.attributes = kJSPropertyAttributeDontDelete,
	},{
	}
};

static const JSClassDefinition sysinfo_classdef = {
	.className = "Sysinfo",
	.finalize = sysinfo_finalize,
	.staticFunctions = sysinfo_functions,
};

static JSObjectRef javascript_sysinfo_create(
	JSContextRef js, JSClassRef class,
	struct javascript_userdata *user_data)
{
	struct sysinfo *inf;

	inf = sysinfo_new(js, user_data);
	if (!inf)
		return NULL;

	return JSObjectMake(js, class, inf);
}

struct javascript_module javascript_sysinfo = {
	.classdef = &sysinfo_classdef,
	.create = javascript_sysinfo_create,
};
