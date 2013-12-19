bin_PROGRAMS = \
	remote-control-browser

remote_control_browser_CPPFLAGS = \
	-DSYSCONF_DIR=\"$(sysconfdir)\" \
	-DPKG_DATA_DIR=\"$(pkgdatadir)\" \
	-I$(top_srcdir)/src/common \
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
	adblock.c \
	adblock.h \
	main.c \
	jshooks.c \
	jshooks.h \
	webkit-browser.c \
	webkit-browser.h \
	webkit-browser-tab-label.c \
	webkit-browser-tab-label.h

if !ENABLE_WEBKIT2
remote_control_browser_SOURCES += \
	katze-scrolled.c \
	katze-scrolled.h
endif

remote_control_browser_LDADD = \
	../../src/common/libcommon.la \
	../utils/libutils.la \
	@POPPLER_LIBS@ \
	@WEBKIT_LIBS@ \
	@GTKOSK_LIBS@ \
	@GTK_LIBS@

jshooksdir = $(pkgdatadir)/jshooks
jshooks_DATA = \
	jshooks/post-global-.js

EXTRA_DIST = \
	$(jshooks_DATA)
