INCLUDES = -I$(top_srcdir)/src/common

bin_PROGRAMS = \
	remote-control-browser

remote_control_browser_CPPFLAGS = \
	@WEBKIT_CFLAGS@ \
	@GTKOSK_CFLAGS@ \
	@GTK_CFLAGS@

remote_control_browser_SOURCES = \
	gtk-drag-view.c \
	gtk-drag-view.h \
	main.c \
	webkit-browser.c \
	webkit-browser.h

remote_control_browser_LDADD = \
	../../src/common/libcommon.la \
	@WEBKIT_LIBS@ \
	@GTKOSK_LIBS@ \
	@GTK_LIBS@
