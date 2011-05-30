bin_PROGRAMS = \
	remote-control

remote_control_CPPFLAGS = \
	-DSYSCONF_DIR=\"$(sysconfdir)\" \
	-I$(top_srcdir)/src/common \
	-I$(top_srcdir)/src/server

remote_control_CFLAGS = \
	@GLIB_CFLAGS@ \
	@LIBRPC_CFLAGS@ \
	@LIBNL_CFLAGS@ \
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
	@GLIB_LIBS@ \
	@LIBRPC_LIBS@ \
	@LIBNL_LIBS@ -lnl-route \
	@WEBKIT_LIBS@ \
	@ALSA_LIBS@
