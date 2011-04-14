/*
 * Copyright (C) 2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef GTK_DRAG_VIEW_H
#define GTK_DRAG_VIEW_H 1

G_BEGIN_DECLS

#define GTK_TYPE_DRAG_VIEW    (gtk_drag_view_get_type())
#define GTK_IS_DRAG_VIEW(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_DRAG_VIEW))

typedef struct GtkDragView      GtkDragView;
typedef struct GtkDragViewClass GtkDragViewClass;

struct GtkDragView {
	GtkBin parent;
};

struct GtkDragViewClass {
	GtkBinClass parent_class;
};

GType gtk_drag_view_get_type(void) G_GNUC_CONST;
GtkWidget *gtk_drag_view_new(GtkAdjustment *hadjustment,
		GtkAdjustment *vadjustment);

G_END_DECLS

#endif /* GTK_DRAG_VIEW_H */
