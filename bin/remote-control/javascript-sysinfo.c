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

struct sysinfo {
	/* remove it if struct is not empty */
	int dummy;
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

static struct sysinfo *sysinfo_new(JSContextRef context,
	struct javascript_userdata *data)
{
	struct sysinfo *inf = g_new0(struct sysinfo, 1);

	if (!inf) {
		g_warning("js-request: failed to allocate memory");
		return NULL;
	}

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
