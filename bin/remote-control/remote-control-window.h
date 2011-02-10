/*
 * Copyright (C) 2010-2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef REMOTE_CONTROL_WINDOW_H
#define REMOTE_CONTROL_WINDOW_H 1

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define REMOTE_CONTROL_TYPE_WINDOW            (remote_control_window_get_type())
#define REMOTE_CONTROL_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), REMOTE_CONTROL_TYPE_WINDOW, RemoteControlWindow))
#define REMOTE_CONTROL_IS_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), REMOTE_CONTROL_TYPE_WINDOW))
#define REMOTE_CONTROL_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), REMOTE_CONTROL_TYPE_WINDOW, RemoteControlWindowClass))
#define REMOTE_CONTROL_IS_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), REMOTE_CONTROL_TYPE_WINDOW))
#define REMOTE_CONTROL_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), REMOTE_CONTROL_TYPE_WINDOW, RemoteControlWindowClass))

typedef struct _RemoteControlWindow        RemoteControlWindow;
typedef struct _RemoteControlWindowPrivate RemoteControlWindowPrivate;
typedef struct _RemoteControlWindowClass   RemoteControlWindowClass;

struct _RemoteControlWindow {
	GtkWindow parent;
};

struct _RemoteControlWindowClass {
	GtkWindowClass parent;
};

GType remote_control_window_get_type(void);

GtkWidget *remote_control_window_new(GMainContext *context);
gboolean remote_control_window_connect(RemoteControlWindow *window,
		const gchar *hostname, const gchar *username,
		const gchar *password, guint delay);
gboolean remote_control_window_reconnect(RemoteControlWindow *window);
gboolean remote_control_window_disconnect(RemoteControlWindow *window);

G_END_DECLS

#endif /* REMOTE_CONTROL_WINDOW_H */
