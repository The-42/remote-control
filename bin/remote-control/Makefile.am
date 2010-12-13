bin_PROGRAMS = \
	remote-control

remote_control_CFLAGS = \
	-I $(top_srcdir)/src/server \
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
