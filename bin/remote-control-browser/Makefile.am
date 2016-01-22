bin_PROGRAMS = \
	remote-control-browser

BUILT_SOURCES = \
	gtkosk-dbus.c \
	gtkosk-dbus.h
CLEANFILES = \
	gtkosk-dbus.c \
	gtkosk-dbus.h

remote_control_browser_CPPFLAGS = \
	-DSYSCONF_DIR=\"$(sysconfdir)\" \
	-DPKG_DATA_DIR=\"$(pkgdatadir)\" \
	-I$(top_srcdir)/src/common \
	-I$(top_srcdir)/bin/utils \
	@POPPLER_CFLAGS@ \
	@WEBKIT_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@GTK_CFLAGS@

remote_control_browser_SOURCES = \
	gtkosk-dbus.c \
	gtk-drag-view.c \
	gtk-drag-view.h \
	gtk-pdf-view.c \
	gtk-pdf-view.h \
	main.c \
	jshooks.c \
	jshooks.h \
	webkit-browser.c \
	webkit-browser.h \
	webkit-browser-tab-label.c \
	webkit-browser-tab-label.h

if !ENABLE_GTK3
remote_control_browser_SOURCES += \
	katze-scrolled.c \
	katze-scrolled.h
endif

if !ENABLE_WEBKIT2
remote_control_browser_SOURCES += \
	adblock.c \
	adblock.h
endif

gtkosk-dbus.c: gtkosk-dbus.h
	touch $@

gtkosk-dbus.h:
	gdbus-codegen \
		--generate-c-code gtkosk-dbus \
		--c-namespace GtkOsk \
		--interface-prefix com.avionicdesign.gtkosk. \
		@GTKOSK_DATADIR@/com.avionicdesign.gtkosk.control.xml

remote_control_browser_LDADD = \
	../../src/common/libcommon.la \
	../utils/libutils.la \
	@POPPLER_LIBS@ \
	@WEBKIT_LIBS@ \
	@GLIB_LIBS@ \
	@GTK_LIBS@

jshooksdir = $(pkgdatadir)/jshooks
jshooks_DATA = \
	jshooks/post-youtube.com.js

EXTRA_DIST = \
	$(jshooks_DATA)
