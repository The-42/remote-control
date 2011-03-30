/*
 * Copyright (C) 2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef REMOTE_CONTROL_SCROLLED_WINDOW_H
#define REMOTE_CONTROL_SCROLLED_WINDOW_H 1

G_BEGIN_DECLS

#define REMOTE_CONTROL_TYPE_SCROLLED_WINDOW    (remote_control_scrolled_window_get_type())
#define REMOTE_CONTROL_IS_SCROLLED_WINDOW(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), REMOTE_CONTROL_TYPE_SCROLLED_WINDOW))

typedef struct RemoteControlScrolledWindow      RemoteControlScrolledWindow;
typedef struct RemoteControlScrolledWindowClass RemoteControlScrolledWindowClass;

struct RemoteControlScrolledWindow {
	GtkBin parent;
};

struct RemoteControlScrolledWindowClass {
	GtkBinClass parent_class;
};

GType remote_control_scrolled_window_get_type(void) G_GNUC_CONST;
GtkWidget *remote_control_scrolled_window_new(GtkAdjustment *hadjustment,
		GtkAdjustment *vadjustment);

G_END_DECLS

#endif /* REMOTE_CONTROL_SCROLLED_WINDOW_H */
