VERSION_INFO = 3:0:3

include_HEADERS = \
	remote-control.h

lib_LTLIBRARIES = \
	libremote-control.la

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = remote-control.pc

libremote_control_la_CPPFLAGS = \
	@VISIBILITY_CFLAGS@ \
	@LIBRPC_CFLAGS@ \
	@GLIB_CFLAGS@

libremote_control_la_SOURCES = \
	audio.c \
	backlight.c \
	card.c \
	events.c \
	gpio.c \
	handset.c \
	irq.c \
	lldp.c \
	media-player.c \
	mixer.c \
	modem.c \
	remote-control.c \
	remote-control.h \
	sound-manager.c \
	task-manager.c \
	tuner.c \
	voip.c

nodist_libremote_control_la_SOURCES = \
	remote-control-stub.c \
	remote-control-stub.h

libremote_control_la_LDFLAGS = -version-info $(VERSION_INFO)

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
