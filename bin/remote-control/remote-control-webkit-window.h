/*
 * Copyright (C) 2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef REMOTE_CONTROL_WEBKIT_WINDOW_H
#define REMOTE_CONTROL_WEBKIT_WINDOW_H 1

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define REMOTE_CONTROL_TYPE_WEBKIT_WINDOW            (remote_control_webkit_window_get_type())
#define REMOTE_CONTROL_WEBKIT_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), REMOTE_CONTROL_TYPE_WEBKIT_WINDOW, RemoteControlWebkitWindow))
#define REMOTE_CONTROL_IS_WEBKIT_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), REMOTE_CONTROL_TYPE_WEBKIT_WINDOW))
#define REMOTE_CONTROL_WEBKIT_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), REMOTE_CONTROL_TYPE_WEBKIT_WINDOW, RemoteControlWebkitWindowClass))
#define REMOTE_CONTROL_IS_WEBKIT_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), REMOTE_CONTROL_TYPE_WEBKIT_WINDOW))
#define REMOTE_CONTROL_WEBKIT_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), REMOTE_CONTROL_TYPE_WEBKIT_WINDOW, RemoteControlWebkitWindowClass))

typedef struct _RemoteControlWebkitWindow        RemoteControlWebkitWindow;
typedef struct _RemoteControlWebkitWindowPrivate RemoteControlWebkitWindowPrivate;
typedef struct _RemoteControlWebkitWindowClass   RemoteControlWebkitWindowClass;

struct _RemoteControlWebkitWindow {
	GtkWindow parent;
};

struct _RemoteControlWebkitWindowClass {
	GtkWindowClass parent;
};

GType remote_control_webkit_window_get_type(void);

GtkWidget *remote_control_webkit_window_new(GMainLoop *loop, gboolean inspector);
gboolean remote_control_webkit_window_load(RemoteControlWebkitWindow *self,
		const gchar *uri);
gboolean remote_control_webkit_window_reload(RemoteControlWebkitWindow *self);

G_END_DECLS

#endif /* REMOTE_CONTROL_WEBKIT_WINDOW_H */
