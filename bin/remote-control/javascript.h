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
#include <webkit/webkit.h>

#ifdef __cplusplus
extern "C" {
#endif

int javascript_register_input_class(void);
int javascript_register_input(JSContextRef js, GMainContext *context,
		JSObjectRef parent, const char *name);


int javascript_register(WebKitWebFrame *frame, GMainContext *context);

#ifdef __cplusplus
}
#endif

#endif /* JAVASCRIPT_API_H */
