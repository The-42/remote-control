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

#include <string.h>

#include <gtk/gtk.h>

#include "gtk-drag-view.h"

typedef struct {
	GtkAdjustment *hadjustment;
	GtkAdjustment *vadjustment;
	gboolean pressed;
	gdouble x;
	gdouble y;
} GtkDragViewPrivate;

#define GTK_DRAG_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), GTK_TYPE_DRAG_VIEW, GtkDragViewPrivate))

G_DEFINE_TYPE(GtkDragView, gtk_drag_view, GTK_TYPE_BIN);

static gboolean on_button_press(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	GtkDragViewPrivate *priv = GTK_DRAG_VIEW_GET_PRIVATE(data);

	g_return_val_if_fail(event->type == GDK_BUTTON_PRESS, FALSE);

	priv->x = event->button.x;
	priv->y = event->button.y;
	priv->pressed = TRUE;

	return FALSE;
}

static gboolean on_button_release(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	GtkDragViewPrivate *priv = GTK_DRAG_VIEW_GET_PRIVATE(data);

	g_return_val_if_fail(event->type == GDK_BUTTON_RELEASE, FALSE);

	priv->pressed = FALSE;

	return FALSE;
}

static gboolean on_motion_notify(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	GtkDragViewPrivate *priv = GTK_DRAG_VIEW_GET_PRIVATE(data);
	gdouble lower;
	gdouble upper;
	gdouble value;
	gdouble size;
	gdouble dx;
	gdouble dy;

	g_return_val_if_fail(event->type == GDK_MOTION_NOTIFY, FALSE);

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

static void gtk_drag_view_add(GtkContainer *container, GtkWidget *child)
{
	GtkDragViewPrivate *priv = GTK_DRAG_VIEW_GET_PRIVATE(container);

	GTK_CONTAINER_CLASS(gtk_drag_view_parent_class)->add(container, child);

	g_signal_connect(G_OBJECT(child), "button-press-event", G_CALLBACK(on_button_press), container);
	g_signal_connect(G_OBJECT(child), "button-release-event", G_CALLBACK(on_button_release), container);
	g_signal_connect(G_OBJECT(child), "motion-notify-event", G_CALLBACK(on_motion_notify), container);

#if GTK_CHECK_VERSION(2, 91, 2)
	if (GTK_IS_SCROLLABLE(child))
		gtk_scrollable_set_hadjustment(GTK_SCROLLABLE(child), priv->hadjustment);

	if (GTK_IS_SCROLLABLE(child))
		gtk_scrollable_set_vadjustment(GTK_SCROLLABLE(child), priv->vadjustment);
#else
	gtk_widget_set_scroll_adjustments(child, priv->hadjustment, priv->vadjustment);
#endif
}

struct gtk_drag_view_result {
	GtkWidget *child;
	gboolean found;
};

static void gtk_drag_view_has_child(GtkWidget *widget, gpointer data)
{
	struct gtk_drag_view_result *result = data;

	if (widget == result->child)
		result->found = TRUE;
}

static void gtk_drag_view_remove(GtkContainer *container, GtkWidget *child)
{
	struct gtk_drag_view_result result;

	g_return_if_fail(GTK_IS_DRAG_VIEW(container));
	g_return_if_fail(child != NULL);

	memset(&result, 0, sizeof(result));
	result.child = child;
	result.found = FALSE;

	gtk_container_foreach(container, gtk_drag_view_has_child, &result);
	g_return_if_fail(result.found == TRUE);

#if GTK_CHECK_VERSION(2, 91, 2)
	if (GTK_IS_SCROLLABLE(child))
		gtk_scrollable_set_vadjustment(GTK_SCROLLABLE(child), NULL);

	if (GTK_IS_SCROLLABLE(child))
		gtk_scrollable_set_hadjustment(GTK_SCROLLABLE(child), NULL);
#else
	gtk_widget_set_scroll_adjustments(child, NULL, NULL);
#endif

	g_signal_handlers_disconnect_by_func(child, on_motion_notify, container);
	g_signal_handlers_disconnect_by_func(child, on_button_release, container);
	g_signal_handlers_disconnect_by_func(child, on_button_press, container);

	GTK_CONTAINER_CLASS(gtk_drag_view_parent_class)->remove(container, child);
}

static void gtk_drag_view_init(GtkDragView *window)
{
	GtkDragViewPrivate *priv = GTK_DRAG_VIEW_GET_PRIVATE(window);
#if !GTK_CHECK_VERSION(2, 91, 0)
	GtkObject *adjustment;
#endif

	gtk_widget_set_has_window(GTK_WIDGET(window), FALSE);
	gtk_widget_set_can_focus(GTK_WIDGET(window), TRUE);

#if GTK_CHECK_VERSION(2, 91, 0)
	priv->hadjustment = gtk_adjustment_new(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
	priv->vadjustment = gtk_adjustment_new(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
#else
	adjustment = gtk_adjustment_new(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
	priv->hadjustment = GTK_ADJUSTMENT(adjustment);

	adjustment = gtk_adjustment_new(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
	priv->vadjustment = GTK_ADJUSTMENT(adjustment);
#endif
}

#if !GTK_CHECK_VERSION(2, 91, 5)
static void gtk_drag_view_size_request(GtkWidget *widget, GtkRequisition *requisition)
{
	requisition->width = 0;
	requisition->height = 0;
}
#endif

static void children_allocate(GtkWidget *widget, gpointer data)
{
	GtkAllocation *allocation = data;

	if (gtk_widget_get_visible(widget))
		gtk_widget_size_allocate(widget, allocation);
}

static void gtk_drag_view_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
	gtk_container_foreach(GTK_CONTAINER(widget), children_allocate, allocation);
	gtk_widget_set_allocation(widget, allocation);
}

#if GTK_CHECK_VERSION(2, 91, 0)
static gboolean gtk_drag_view_draw(GtkWidget *widget, cairo_t *cr)
{
	if (gtk_widget_is_drawable(widget))
		GTK_WIDGET_CLASS(gtk_drag_view_parent_class)->draw(widget, cr);

	return FALSE;
}
#else
static gboolean gtk_drag_view_expose(GtkWidget *widget, GdkEventExpose *event)
{
	if (gtk_widget_is_drawable(widget))
		GTK_WIDGET_CLASS(gtk_drag_view_parent_class)->expose_event(widget, event);

	return FALSE;
}
#endif

static void gtk_drag_view_class_init(GtkDragViewClass *class)
{
	GtkContainerClass *container = GTK_CONTAINER_CLASS(class);
	GtkWidgetClass *widget = GTK_WIDGET_CLASS(class);

	container->add = gtk_drag_view_add;
	container->remove = gtk_drag_view_remove;

#if GTK_CHECK_VERSION(2, 91, 0)
	widget->draw = gtk_drag_view_draw;
#else
	widget->expose_event = gtk_drag_view_expose;
#endif
#if !GTK_CHECK_VERSION(2, 91, 5)
	widget->size_request = gtk_drag_view_size_request;
#endif
	widget->size_allocate = gtk_drag_view_size_allocate;

	g_type_class_add_private(class, sizeof(GtkDragViewPrivate));
}

GtkWidget *gtk_drag_view_new(GtkAdjustment *hadjustment, GtkAdjustment *vadjustment)
{
	return g_object_new(GTK_TYPE_DRAG_VIEW, NULL);
}
