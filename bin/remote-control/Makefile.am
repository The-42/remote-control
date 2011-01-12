bin_PROGRAMS = \
	remote-control

remote_control_CPPFLAGS = \
	-DSYSCONF_DIR=\"$(sysconfdir)\" \
	-I $(top_srcdir)/src/server

remote_control_CFLAGS = \
	@GLIB_GIO_CFLAGS@ \
	@LIBRPC_CFLAGS@ \
	@WEBKIT_CFLAGS@ \
	@ALSA_CFLAGS@ \
	@GTK_CFLAGS@

remote_control_SOURCES = \
	remote-control.c \
	remote-control-window.c \
	remote-control-window.h

remote_control_LDADD = \
	../../src/server/libremote-control.la \
	@GLIB_GIO_LIBS@ \
	@LIBRPC_LIBS@ \
	@WEBKIT_LIBS@ \
	@ALSA_LIBS@
