include_HEADERS = \
	remote-control.h

lib_LTLIBRARIES = \
	libremote-control.la

libremote_control_la_CPPFLAGS = \
	@VISIBILITY_CFLAGS@ \
	@GLIB_CFLAGS@

libremote_control_la_SOURCES = \
	backlight.c \
	card.c \
	events.c \
	irq.c \
	lldp.c \
	media-player.c \
	mixer.c \
	remote-control.c \
	remote-control.h \
	voip.c

nodist_libremote_control_la_SOURCES = \
	remote-control-stub.c \
	remote-control-stub.h

libremote_control_la_LIBADD = \
	@GLIB_LIBS@ \
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
