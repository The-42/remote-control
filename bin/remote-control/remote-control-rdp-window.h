/*
 * Copyright (C) 2010-2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef REMOTE_CONTROL_RDP_WINDOW_H
#define REMOTE_CONTROL_RDP_WINDOW_H 1

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define REMOTE_CONTROL_TYPE_RDP_WINDOW            (remote_control_rdp_window_get_type())
#define REMOTE_CONTROL_RDP_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), REMOTE_CONTROL_TYPE_RDP_WINDOW, RemoteControlRdpWindow))
#define REMOTE_CONTROL_IS_RDP_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), REMOTE_CONTROL_TYPE_RDP_WINDOW))
#define REMOTE_CONTROL_RDP_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), REMOTE_CONTROL_TYPE_RDP_WINDOW, RemoteControlRdpWindowClass))
#define REMOTE_CONTROL_IS_RDP_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), REMOTE_CONTROL_TYPE_RDP_WINDOW))
#define REMOTE_CONTROL_RDP_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), REMOTE_CONTROL_TYPE_RDP_WINDOW, RemoteControlRdpWindowClass))

typedef struct _RemoteControlRdpWindow        RemoteControlRdpWindow;
typedef struct _RemoteControlRdpWindowPrivate RemoteControlRdpWindowPrivate;
typedef struct _RemoteControlRdpWindowClass   RemoteControlRdpWindowClass;

struct _RemoteControlRdpWindow {
	GtkWindow parent;
};

struct _RemoteControlRdpWindowClass {
	GtkWindowClass parent;
};

GType remote_control_rdp_window_get_type(void);

GtkWidget *remote_control_rdp_window_new(GMainLoop *loop);
gboolean remote_control_rdp_window_connect(RemoteControlRdpWindow *self,
		const gchar *hostname, const gchar *username,
		const gchar *password, guint delay);
gboolean remote_control_rdp_window_reconnect(RemoteControlRdpWindow *self);
gboolean remote_control_rdp_window_disconnect(RemoteControlRdpWindow *self);

G_END_DECLS

#endif /* REMOTE_CONTROL_RDP_WINDOW_H */
