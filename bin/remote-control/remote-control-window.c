#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include <gdk/gdkx.h>

#include "remote-control-window.h"

G_DEFINE_TYPE(RemoteControlWindow, remote_control_window, GTK_TYPE_WINDOW);

#define REMOTE_CONTROL_WINDOW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), REMOTE_CONTROL_TYPE_WINDOW, RemoteControlWindowPrivate))

struct _RemoteControlWindowPrivate {
	GtkWidget *socket;
	GMainLoop *loop;
	GSource *watch;
	GPid xfreerdp;
};

enum {
	PROP_0,
	PROP_LOOP,
};

static void remote_control_window_get_property(GObject *object, guint prop_id,
		GValue *value, GParamSpec *pspec)
{
	RemoteControlWindow *window = REMOTE_CONTROL_WINDOW(object);
	RemoteControlWindowPrivate *priv;

	priv = REMOTE_CONTROL_WINDOW_GET_PRIVATE(window);

	switch (prop_id) {
	case PROP_LOOP:
		g_value_set_pointer(value, priv->loop);
		break;

	default:
		g_assert_not_reached();
	}
}

static void remote_control_window_set_property(GObject *object, guint prop_id,
		const GValue *value, GParamSpec *pspec)
{
	RemoteControlWindow *window = REMOTE_CONTROL_WINDOW(object);
	RemoteControlWindowPrivate *priv;

	priv = REMOTE_CONTROL_WINDOW_GET_PRIVATE(window);

	switch (prop_id) {
	case PROP_LOOP:
		priv->loop = g_value_get_pointer(value);
		break;

	default:
		g_assert_not_reached();
	}
}

static void remote_control_window_finalize(GObject *object)
{
	RemoteControlWindow *window = REMOTE_CONTROL_WINDOW(object);
	RemoteControlWindowPrivate *priv;

	priv = REMOTE_CONTROL_WINDOW_GET_PRIVATE(window);

	remote_control_window_disconnect(window);

	G_OBJECT_CLASS(remote_control_window_parent_class)->finalize(object);
}

static void remote_control_window_class_init(RemoteControlWindowClass *klass)
{
	GObjectClass *object = G_OBJECT_CLASS(klass);

	g_type_class_add_private(klass, sizeof(RemoteControlWindowPrivate));

	object->get_property = remote_control_window_get_property;
	object->set_property = remote_control_window_set_property;
	object->finalize = remote_control_window_finalize;

	g_object_class_install_property(object, PROP_LOOP,
			g_param_spec_pointer("loop", "application main loop",
				"Application main loop to integrate with.",
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
				G_PARAM_STATIC_STRINGS));
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

	priv->socket = gtk_socket_new();
	gtk_container_add(GTK_CONTAINER(window), priv->socket);
}

GtkWidget *remote_control_window_new(GMainLoop *loop)
{
	return g_object_new(REMOTE_CONTROL_TYPE_WINDOW, "loop", loop, NULL);
}

static void child_watch(GPid pid, gint status, gpointer data)
{
	RemoteControlWindowPrivate *priv = data;

	g_source_destroy(priv->watch);
	g_spawn_close_pid(pid);
	priv->xfreerdp = 0;
}

gboolean remote_control_window_connect(RemoteControlWindow *self,
		const gchar *hostname, const gchar *username,
		const gchar *password)
{
	RemoteControlWindowPrivate *priv;
	GMainContext *context;
	GError *error = NULL;
	gchar **argv;
	XID xid;

	priv = REMOTE_CONTROL_WINDOW_GET_PRIVATE(self);
	xid = gtk_socket_get_id(GTK_SOCKET(priv->socket));

	argv = g_new0(gchar *, 9);
	if (!argv) {
		g_error("g_new0() failed");
		return FALSE;
	}

	argv[0] = g_strdup("xfreerdp");
	argv[1] = g_strdup("-u");
	argv[2] = g_strdup(username);
	argv[3] = g_strdup("-p");
	argv[4] = g_strdup(password);
	argv[5] = g_strdup("-X");
	argv[6] = g_strdup_printf("%lx", xid);
	argv[7] = g_strdup(hostname);
	argv[8] = NULL;

	if (!g_spawn_async(NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD |
			G_SPAWN_SEARCH_PATH, NULL, NULL, &priv->xfreerdp,
			&error)) {
		g_error("g_spawn_async(): %s", error->message);
		g_error_free(error);
		g_strfreev(argv);
		return FALSE;
	}

	g_strfreev(argv);

	priv->watch = g_child_watch_source_new(priv->xfreerdp);
	if (!priv->watch) {
		g_error("g_child_watch_source_new() failed");
		return FALSE;
	}

	g_source_set_callback(priv->watch, (GSourceFunc)child_watch, priv,
			NULL);
	context = g_main_loop_get_context(priv->loop);
	g_source_attach(priv->watch, context);

	return TRUE;
}

gboolean remote_control_window_disconnect(RemoteControlWindow *window)
{
	RemoteControlWindowPrivate *priv;

	priv = REMOTE_CONTROL_WINDOW_GET_PRIVATE(window);

	if (priv->xfreerdp) {
		pid_t pid = priv->xfreerdp;
		int status = 0;
		int err;

		err = kill(pid, SIGTERM);
		if (err < 0) {
			fprintf(stderr, "kill(): %s\n", strerror(errno));
			return FALSE;
		}

		err = waitpid(pid, &status, 0);
		if (err < 0) {
			fprintf(stderr, "waitpid(): %s\n", strerror(errno));
			return FALSE;
		}

		g_debug("xfreerdp exited: %d", WEXITSTATUS(status));
		g_source_destroy(priv->watch);
		g_spawn_close_pid(pid);
		priv->xfreerdp = 0;
	}

	return TRUE;
}
