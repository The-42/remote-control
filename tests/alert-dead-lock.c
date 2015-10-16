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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef USE_WEBKIT2
#include <webkit2/webkit2.h>
#else
#include <webkit/webkit.h>
#endif

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

#if !GTK_CHECK_VERSION(3, 6, 0)
	gdk_threads_init();
#endif

	gtk_init(&argc, &argv);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

	webkit = webkit_web_view_new();
	gtk_container_add(GTK_CONTAINER(window), webkit);

#ifdef USE_WEBKIT2
	webkit_web_view_load_html(WEBKIT_WEB_VIEW(webkit), test_html, NULL);
#else
	webkit_web_view_load_html_string(WEBKIT_WEB_VIEW(webkit),
			test_html, NULL);
#endif

	gtk_widget_show_all(window);

#if !GTK_CHECK_VERSION(3, 6, 0)
	gdk_threads_enter();
	gtk_main();
	gdk_threads_leave();
#else
	gtk_main();
#endif

	return 0;
}
