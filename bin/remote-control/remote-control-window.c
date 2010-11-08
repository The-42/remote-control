#include <webkit/webkit.h>

#include "remote-control-window.h"

G_DEFINE_TYPE(RemoteControlWindow, remote_control_window, GTK_TYPE_WINDOW);

#define REMOTE_CONTROL_WINDOW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), REMOTE_CONTROL_TYPE_WINDOW, RemoteControlWindowPrivate))

struct _RemoteControlWindowPrivate {
	WebKitWebView *webkit;
	GtkWindow *video;
};

static void remote_control_window_finalize(GObject *object)
{
	RemoteControlWindow *window = REMOTE_CONTROL_WINDOW(object);
	RemoteControlWindowPrivate *priv;

	g_debug("> %s(object=%p)", __func__, object);

	priv = REMOTE_CONTROL_WINDOW_GET_PRIVATE(window);
	g_debug("  priv: %p", priv);
	g_debug("    video: %p", priv->video);

	if (priv->video)
		gtk_widget_unref(GTK_WIDGET(priv->video));

	G_OBJECT_CLASS(remote_control_window_parent_class)->finalize(object);
	g_debug("< %s()", __func__);
}

static void remote_control_window_class_init(RemoteControlWindowClass *klass)
{
	GObjectClass *object = G_OBJECT_CLASS(klass);

	g_type_class_add_private(klass, sizeof(RemoteControlWindowPrivate));

	object->finalize = remote_control_window_finalize;
}

static void remote_control_window_init(RemoteControlWindow *self)
{
	GtkWindow *window = GTK_WINDOW(self);
	RemoteControlWindowPrivate *priv;
	GdkScreen *screen;
	gint cx;
	gint cy;

	priv = REMOTE_CONTROL_WINDOW_GET_PRIVATE(self);

	screen = gtk_window_get_screen(window);
	cx = gdk_screen_get_width(screen);
	cy = gdk_screen_get_height(screen);

	gtk_widget_set_size_request(GTK_WIDGET(window), cx, cy);

	priv->webkit = WEBKIT_WEB_VIEW(webkit_web_view_new());

	gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(priv->webkit));
}

GtkWidget *remote_control_window_new(void)
{
	return g_object_new(REMOTE_CONTROL_TYPE_WINDOW, NULL);
}

void remote_control_window_load_uri(RemoteControlWindow *window, const gchar *uri)
{
	RemoteControlWindowPrivate *priv;

	priv = REMOTE_CONTROL_WINDOW_GET_PRIVATE(window);
	webkit_web_view_load_uri(priv->webkit, uri);
}
