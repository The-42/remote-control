/*
 * Copyright (C) 2013 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef JSHOOKS_H
#define JSHOOKS_H 1

#include <gtk/gtk.h>
#include <JavaScriptCore/JavaScript.h>

G_BEGIN_DECLS

gchar **jshooks_determine_hooklist(const gchar *uri, const gchar *prefix);
void jshooks_execute_jscript(JSContextRef js_context, gchar *content, gchar* sname);

G_END_DECLS

#endif /* JSHOOKS_H */
