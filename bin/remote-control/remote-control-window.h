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

GtkWidget *remote_control_window_new(void);
void remote_control_window_load_uri(RemoteControlWindow *window, const gchar *uri);

G_END_DECLS

#endif /* REMOTE_CONTROL_WINDOW_H */
