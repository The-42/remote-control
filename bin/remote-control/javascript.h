/*
 * Copyright (C) 2012 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef JAVASCRIPT_API_H
#define JAVASCRIPT_API_H 1

#include <JavaScriptCore/JavaScript.h>
#include "remote-control-webkit-window.h"
#include "remote-control-data.h"

struct javascript_userdata {
	RemoteControlWebkitWindow *window;
	struct remote_control_data *rcd;
	GMainLoop *loop;
};

struct javascript_module {
	const JSClassDefinition	*classdef;
	JSObjectRef (*create)(JSContextRef js, JSClassRef class,
			struct javascript_userdata *data);

	JSClassRef		class;
};

void javascript_set_exception_text(JSContextRef context,JSValueRef *exception,
				const char *failure);

int javascript_register(JSGlobalContextRef js,
			struct javascript_userdata *user_data);

#endif /* JAVASCRIPT_API_H */
