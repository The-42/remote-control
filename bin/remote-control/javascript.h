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

struct javascript_userdata {
	RemoteControlWebkitWindow *window;
	GMainLoop *loop;
};

#ifdef __cplusplus
extern "C" {
#endif

int javascript_register_cursor_class(void);
int javascript_register_cursor(JSContextRef js, JSObjectRef parent,
                               const char *name, void *user_data);

int javascript_register_input_class(void);
int javascript_register_input(JSContextRef js, JSObjectRef parent,
                              const char *name, void *user_data);

#ifdef ENABLE_JAVASCRIPT_IR
int javascript_register_ir_class(void);
int javascript_register_ir(JSContextRef js, JSObjectRef parent,
                           const char *name, void *user_data);
#else
static inline int javascript_register_ir_class(void)
{
	return 0;
}

static inline int javascript_register_ir(JSContextRef js, JSObjectRef parent,
                                         const char *name, void *user_data)
{
	return 0;
}
#endif
#ifdef ENABLE_JAVASCRIPT_LCD
int javascript_register_lcd_class(void);
int javascript_register_lcd(JSContextRef js, JSObjectRef parent,
                            const char *name, void *user_data);
#else
static inline int javascript_register_lcd_class(void)
{
	return 0;
}

static inline int javascript_register_lcd(JSContextRef js, JSObjectRef parent,
                                          const char *name, void *user_data)
{
	return 0;
}
#endif


int javascript_register(JSGlobalContextRef js,
                        struct javascript_userdata *user_data);

#ifdef __cplusplus
}
#endif

#endif /* JAVASCRIPT_API_H */
