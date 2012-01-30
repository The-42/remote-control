/*
 * Copyright (C) 2011-2012 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _REMOTE_CONTROL_WEBKIT_JSCRIPT__H_
#define _REMOTE_CONTROL_WEBKIT_JSCRIPT__H_ 1

#ifdef ENABLE_JAVASCRIPT

#include <webkit/webkit.h>

G_BEGIN_DECLS

int register_user_functions(WebKitWebView *webkit, GtkWidget *widget);

G_END_DECLS

#endif /* ENABLE_JAVASCRIPT */

#endif /*_REMOTE_CONTROL_WEBKIT_JSCRIPT__H_*/
