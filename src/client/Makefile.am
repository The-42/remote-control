AM_CPPFLAGS = @GLIB_GIO_CFLAGS@

noinst_LTLIBRARIES = \
	libremote-control.la

libremote_control_la_SOURCES = \
	backlight.c \
	events.c \
	media-player.c \
	medcom.c \
	remote-control-stub.c \
	voip.c

libremote_control_la_LIBADD = \
	@GLIB_GIO_LIBS@ \
	@LIBRPC_LIBS@ \
	-lpthread

remote-control-stub.c: $(srcdir)/../protocol.xml
	$(AM_V_GEN)$(RPC_STUBGEN) --client --source -o $@ $<

remote-control-stub.h: $(srcdir)/../protocol.xml
	$(AM_V_GEN)$(RPC_STUBGEN) --client --header -o $@ $<

BUILT_SOURCES = \
	remote-control-stub.c \
	remote-control-stub.h

CLEANFILES = \
	$(BUILT_SOURCES)
