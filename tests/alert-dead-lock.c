/*
 * Build this file with the following command:
 *
 * gcc -O2 -g -Wall -Werror `pkg-config --cflags --libs webkit-1.0` \
 *				-o alert-dead-lock alert-dead-lock.c
 *
 * Or:
 *
 * gcc -O2 -g -Wall -Werror `pkg-config --cflags --libs webkitgtk-3.0` \
 *			`pkg-config --cflags --libs javascriptcoregtk-3.0` \
 *			-o alert-dead-lock alert-dead-lock.c
 */

#include <webkit/webkit.h>
#include <gtk/gtk.h>

static const char test_html[] =
	"<!doctype html>\n"
	"<html>\n"
	"  <head>\n"
	"    <title>Alert dead-lock</title>\n"
	"  </head>\n"
	"  <body onload=\"alert('Hello, World')\">\n"
	"  </body>\n"
	"</html>\n";

int main(int argc, char *argv[])
{
	GtkWidget *window;
	GtkWidget *webkit;

#if !GLIB_CHECK_VERSION(2, 31, 0)
	if (!g_thread_supported())
		g_thread_init(NULL);
#endif

	gdk_threads_init();

	gtk_init(&argc, &argv);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

	webkit = webkit_web_view_new();
	gtk_container_add(GTK_CONTAINER(window), webkit);

	webkit_web_view_load_html_string(WEBKIT_WEB_VIEW(webkit),
			test_html, NULL);

	gtk_widget_show_all(window);

	gdk_threads_enter();
	gtk_main();
	gdk_threads_leave();

	return 0;
}
