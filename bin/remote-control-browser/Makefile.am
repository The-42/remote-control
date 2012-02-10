INCLUDES = -I$(top_srcdir)/src/common

bin_PROGRAMS = \
	remote-control-browser

remote_control_browser_CPPFLAGS = \
	-DSYSCONF_DIR=\"$(sysconfdir)\" \
	-I$(top_srcdir)/bin/utils \
	@POPPLER_CFLAGS@ \
	@WEBKIT_CFLAGS@ \
	@GTKOSK_CFLAGS@ \
	@GTK_CFLAGS@

remote_control_browser_SOURCES = \
	gtk-drag-view.c \
	gtk-drag-view.h \
	gtk-pdf-view.c \
	gtk-pdf-view.h \
	main.c \
	webkit-browser.c \
	webkit-browser.h \
	webkit-browser-tab-label.c \
	webkit-browser-tab-label.h

remote_control_browser_LDADD = \
	../../src/common/libcommon.la \
	../utils/libutils.la \
	@POPPLER_LIBS@ \
	@WEBKIT_LIBS@ \
	@GTKOSK_LIBS@ \
	@GTK_LIBS@
