/*
 * Build this file with the following command:
 *
 * gcc -O2 -g -Wall -Werror `pkg-config --cflags --libs webkit-1.0` \
 *				-o ajax-dead-lock ajax-dead-lock.c
 *
 * Or:
 *
 * gcc -O2 -g -Wall -Werror `pkg-config --cflags --libs webkitgtk-3.0` \
 *			`pkg-config --cflags --libs javascriptcoregtk-3.0` \
 *			-o ajax-dead-lock ajax-dead-lock.c
 */

#include <webkit/webkit.h>
#include <gtk/gtk.h>

static const char test_html[] =
	"<!doctype html>\n"
	"<html>\n"
	"  <head>\n"
	"    <title>AJAX dead-lock</title>\n"
	"    <script type=\"text/javascript\">\n"
	"      function request_get_async(url)\n"
	"      {\n"
	"        request = new XMLHttpRequest();\n"
	"        if (!request)\n"
	"          return false;\n"
	"\n"
	"        request.onreadystatechange = function() {\n"
	"          element = document.getElementById('response');\n"
	"          message = 'Status: ' + request.status;\n"
	"\n"
	"          if (request.readyState == 4)\n"
	"            message += '<br/><code>' + request.responseText + '</code>';\n"
	"\n"
	"          element.innerHTML = message;\n"
	"        }\n"
	"\n"
	"        request.open('GET', url, true);\n"
	"        request.send();\n"
	"      }\n"
	"\n"
	"      function request_get_sync(url)\n"
	"      {\n"
	"        request = new XMLHttpRequest();\n"
	"        if (!request)\n"
	"          return false;\n"
	"\n"
	"        request.open('GET', url, false);\n"
	"        request.send();\n"
	"\n"
	"        return request.responseText;\n"
	"      }\n"
	"\n"
	"      function request_immediate(async)\n"
	"      {\n"
	"        uri = 'http://unionplatform.com/xmlhttptest/echo.php';\n"
	"\n"
	"        if (async)\n"
	"          request_get_async(uri);\n"
	"        else {\n"
	"          element = document.getElementById('response');\n"
	"          response = request_get_sync(uri);\n"
	"          element.innerHTML = response;\n"
	"        }\n"
	"      }\n"
	"\n"
	"      function request_delay(async)\n"
	"      {\n"
	"        setTimeout('request_immediate(' + async + ')', 1000);\n"
	"      }\n"
	"    </script>\n"
	"  </head>\n"
	"  <body>\n"
	"    <table>\n"
	"      <tr><td>Asynchronous</td><td>Synchronous</td></tr>\n"
	"      <tr>\n"
	"        <td>\n"
	"          <a href=\"javascript:request_immediate(true)\">Immediate</a>\n"
	"          <a href=\"javascript:request_delay(true)\">Delay</a>\n"
	"        </td>\n"
	"        <td>\n"
	"          <a href=\"javascript:request_immediate(false)\">Immediate</a>\n"
	"          <a href=\"javascript:request_delay(false)\">Delay</a>\n"
	"        </td>\n"
	"      </tr>\n"
	"      <tr><td id=\"status\" colspan=\"2\"/></tr>\n"
	"      <tr><td id=\"response\" colspan=\"2\"/></tr>\n"
	"    </table>\n"
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
