/*
 * Copyright (C) 2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gtk/gtk.h>

#include "remote-control-scrolled-window.h"

typedef struct {
	GtkAdjustment *hadjustment;
	GtkAdjustment *vadjustment;
	gboolean pressed;
	gdouble x;
	gdouble y;
} RemoteControlScrolledWindowPrivate;

#define REMOTE_CONTROL_SCROLLED_WINDOW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), REMOTE_CONTROL_TYPE_SCROLLED_WINDOW, RemoteControlScrolledWindowPrivate))

G_DEFINE_TYPE(RemoteControlScrolledWindow, remote_control_scrolled_window, GTK_TYPE_BIN);

static gboolean on_button_press(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	RemoteControlScrolledWindowPrivate *priv;

	g_return_val_if_fail(event->type == GDK_BUTTON_PRESS, FALSE);

	priv = REMOTE_CONTROL_SCROLLED_WINDOW_GET_PRIVATE(data);
	priv->x = event->button.x;
	priv->y = event->button.y;
	priv->pressed = TRUE;

	return FALSE;
}

static gboolean on_button_release(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	RemoteControlScrolledWindowPrivate *priv;

	g_return_val_if_fail(event->type == GDK_BUTTON_RELEASE, FALSE);

	priv = REMOTE_CONTROL_SCROLLED_WINDOW_GET_PRIVATE(data);
	priv->pressed = FALSE;

	return FALSE;
}

static gboolean on_motion_notify(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	RemoteControlScrolledWindowPrivate *priv;
	GtkWidget *scroll = data;
	gdouble lower;
	gdouble upper;
	gdouble value;
	gdouble size;
	gdouble dx;
	gdouble dy;

	g_return_val_if_fail(event->type == GDK_MOTION_NOTIFY, FALSE);

	priv = REMOTE_CONTROL_SCROLLED_WINDOW_GET_PRIVATE(scroll);

	if (priv->pressed) {
		dx = event->motion.x - priv->x;
		dy = event->motion.y - priv->y;

		size = gtk_adjustment_get_page_size(priv->hadjustment);
		lower = gtk_adjustment_get_lower(priv->hadjustment);
		upper = gtk_adjustment_get_upper(priv->hadjustment);
		value = gtk_adjustment_get_value(priv->hadjustment);
		value = CLAMP(value - dx, lower, upper - size);
		gtk_adjustment_set_value(priv->hadjustment, value);

		size = gtk_adjustment_get_page_size(priv->vadjustment);
		lower = gtk_adjustment_get_lower(priv->vadjustment);
		upper = gtk_adjustment_get_upper(priv->vadjustment);
		value = gtk_adjustment_get_value(priv->vadjustment);
		value = CLAMP(value - dy, lower, upper - size);
		gtk_adjustment_set_value(priv->vadjustment, value);

		priv->x = event->motion.x;
		priv->y = event->motion.y;
	}

	return priv->pressed;
}

static void remote_control_scrolled_window_add(GtkContainer *container, GtkWidget *child)
{
	RemoteControlScrolledWindowPrivate *priv;
	GtkBin *bin = GTK_BIN(container);

	priv = REMOTE_CONTROL_SCROLLED_WINDOW_GET_PRIVATE(container);

	gtk_widget_set_parent(child, GTK_WIDGET(container));
	bin->child = child;

	g_signal_connect(G_OBJECT(child), "button-press-event", G_CALLBACK(on_button_press), container);
	g_signal_connect(G_OBJECT(child), "button-release-event", G_CALLBACK(on_button_release), container);
	g_signal_connect(G_OBJECT(child), "motion-notify-event", G_CALLBACK(on_motion_notify), container);

	gtk_widget_set_scroll_adjustments(child, priv->hadjustment, priv->vadjustment);
}

static void remote_control_scrolled_window_remove(GtkContainer *container, GtkWidget *child)
{
	GtkBin *bin = GTK_BIN(container);

	g_return_if_fail(REMOTE_CONTROL_IS_SCROLLED_WINDOW(container));
	g_return_if_fail(bin->child == child);
	g_return_if_fail(child != NULL);

	gtk_widget_set_scroll_adjustments(child, NULL, NULL);
	g_signal_handlers_disconnect_by_func(child, on_motion_notify, container);
	g_signal_handlers_disconnect_by_func(child, on_button_release, container);
	g_signal_handlers_disconnect_by_func(child, on_button_press, container);

	GTK_CONTAINER_CLASS(remote_control_scrolled_window_parent_class)->remove(container, child);
}

static void remote_control_scrolled_window_init(RemoteControlScrolledWindow *window)
{
	RemoteControlScrolledWindowPrivate *priv = REMOTE_CONTROL_SCROLLED_WINDOW_GET_PRIVATE(window);
	GtkObject *adjustment;

	gtk_widget_set_has_window(GTK_WIDGET(window), FALSE);
	gtk_widget_set_can_focus(GTK_WIDGET(window), TRUE);

	adjustment = gtk_adjustment_new(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
	priv->hadjustment = GTK_ADJUSTMENT(adjustment);

	adjustment = gtk_adjustment_new(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
	priv->vadjustment = GTK_ADJUSTMENT(adjustment);
}

static void remote_control_scrolled_window_size_request(GtkWidget *widget, GtkRequisition *requisition)
{
	requisition->width = widget->allocation.width;
	requisition->height = widget->allocation.height;
}

static void remote_control_scrolled_window_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
	GtkBin *bin = GTK_BIN(widget);

	if (bin->child && gtk_widget_get_visible(bin->child))
		gtk_widget_size_allocate(bin->child, allocation);

	widget->allocation = *allocation;
}

static gboolean remote_control_scrolled_window_expose(GtkWidget *widget, GdkEventExpose *event)
{
	if (gtk_widget_is_drawable(widget))
		GTK_WIDGET_CLASS(remote_control_scrolled_window_parent_class)->expose_event(widget, event);

	return FALSE;
}

static void remote_control_scrolled_window_class_init(RemoteControlScrolledWindowClass *class)
{
	GtkContainerClass *container = GTK_CONTAINER_CLASS(class);
	GtkWidgetClass *widget = GTK_WIDGET_CLASS(class);

	container->add = remote_control_scrolled_window_add;
	container->remove = remote_control_scrolled_window_remove;

	widget->expose_event = remote_control_scrolled_window_expose;
	widget->size_request = remote_control_scrolled_window_size_request;
	widget->size_allocate = remote_control_scrolled_window_size_allocate;

	g_type_class_add_private(class, sizeof(RemoteControlScrolledWindowPrivate));
}

GtkWidget *remote_control_scrolled_window_new(GtkAdjustment *hadjustment, GtkAdjustment *vadjustment)
{
	return g_object_new(REMOTE_CONTROL_TYPE_SCROLLED_WINDOW, NULL);
}
