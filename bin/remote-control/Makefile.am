bin_PROGRAMS = \
	remote-control

remote_control_CPPFLAGS = \
	-DSYSCONF_DIR=\"$(sysconfdir)\" \
	-I$(top_srcdir)/src/common \
	-I$(top_srcdir)/src/server \
	-I$(top_srcdir)/bin/utils \
	@GLIB_CFLAGS@ \
	@LIBRPC_CFLAGS@ \
	@WEBKIT_CFLAGS@ \
	@ALSA_CFLAGS@ \
	@GTK_CFLAGS@

remote_control_SOURCES = \
	remote-control.c \
	remote-control-rdp-window.c \
	remote-control-rdp-window.h \
	remote-control-webkit-window.c \
	remote-control-webkit-window.h

remote_control_LDADD = \
	../../src/server/libremote-control.la \
	../../src/common/libcommon.la \
	../utils/libutils.la \
	@GLIB_LIBS@ \
	@LIBRPC_LIBS@ \
	@WEBKIT_LIBS@ \
	@ALSA_LIBS@

if ENABLE_JAVASCRIPT
remote_control_CPPFLAGS += \
	@GUDEV_CFLAGS@

remote_control_SOURCES += \
	javascript.c \
	javascript.h \
	javascript-cursor.c \
	javascript-input.c

remote_control_LDADD += \
	@GUDEV_LIBS@

if ENABLE_JAVASCRIPT_IR
remote_control_SOURCES += \
endif
endif
