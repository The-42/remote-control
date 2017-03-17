/*
 * Copyright (C) 2010-2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <unistd.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <poll.h>

#include "remote-control.h"

#define CURSOR_MOVEMENT_SLEEP 100000

struct cursor_movement {
	GThread *thread;
	int timeout;
};

static void cursor_motion_wait_for_motion(struct cursor_movement *priv,
		Display *dpy)
{
	int xfd = ConnectionNumber(dpy);

	while (priv->timeout) {
		struct pollfd fds[1];
		int err;

		fds[0].fd = xfd;
		fds[0].events = POLLIN;

		err = poll(fds, 1, CURSOR_MOVEMENT_SLEEP / 1000);
		if (err < 0 || fds[0].revents & POLLIN || XPending(dpy))
			return;
	}
}

static gpointer cursor_movement_thread(gpointer user_data)
{
	struct cursor_movement *priv = (struct cursor_movement *)user_data;
	Display *dpy = XOpenDisplay(NULL);
	const char data[] = { 0 };
	Cursor emptyCursor = 0;
	XColor color = { 0 };
	Pixmap pixmap = 0;
	Window win;

	if (!dpy)
		return NULL;

	win = RootWindow(dpy, DefaultScreen(dpy));
	pixmap = XCreateBitmapFromData(dpy, win, data, 1, 1);
	emptyCursor = XCreatePixmapCursor(dpy, pixmap, pixmap, &color, &color,
			0, 0);

	while (priv->timeout) {
		const unsigned int mask = PointerMotionMask | ButtonPressMask;
		gint64 start;
		XEvent event;
		int ret;

		ret = XGrabPointer(dpy, win, True, mask, GrabModeSync,
				GrabModeAsync, None, emptyCursor, CurrentTime);
		if (ret != GrabSuccess) {
			usleep(CURSOR_MOVEMENT_SLEEP);
			continue;
		}

		XAllowEvents(dpy, SyncPointer, CurrentTime);
		XSync(dpy, False);

		cursor_motion_wait_for_motion(priv, dpy);

		XAllowEvents(dpy, ReplayPointer, CurrentTime);
		XUngrabPointer(dpy, CurrentTime);
		while (priv->timeout && XPending(dpy))
			XMaskEvent(dpy, mask, &event);
		start = g_get_monotonic_time();
		while (start + 1000LL * priv->timeout > g_get_monotonic_time())
			usleep(CURSOR_MOVEMENT_SLEEP);
	}

	if (pixmap)
		XFreePixmap(dpy, pixmap);
	if (emptyCursor)
		XFreeCursor(dpy, emptyCursor);
	XCloseDisplay(dpy);
	return NULL;
}

int cursor_movement_create(struct cursor_movement **cursor_movement)
{
	if (!cursor_movement)
		return -EINVAL;

	*cursor_movement = g_new0(struct cursor_movement, 1);

	return 0;
}

int cursor_movement_free(struct cursor_movement *cursor_movement)
{
	if (!cursor_movement)
		return -EINVAL;

	cursor_movement_set_timeout(cursor_movement, 0);
	g_free(cursor_movement);

	return 0;
}

int cursor_movement_set_timeout(struct cursor_movement *cursor_movement, int timeout)
{
	if (!cursor_movement)
		return -EINVAL;

	cursor_movement->timeout = timeout;
	if (!cursor_movement->timeout && cursor_movement->thread) {
		g_thread_join(cursor_movement->thread);
		cursor_movement->thread = NULL;
	} else if (cursor_movement->timeout && !cursor_movement->thread) {
		cursor_movement->thread = g_thread_new("cursor_movement_thread",
				cursor_movement_thread, cursor_movement);
		if (!cursor_movement->thread)
			return -ENOMEM;
	}
	return 0;
}

int cursor_movement_get_timeout(struct cursor_movement *cursor_movement)
{
	if (!cursor_movement)
		return -EINVAL;

	return cursor_movement->timeout;
}
