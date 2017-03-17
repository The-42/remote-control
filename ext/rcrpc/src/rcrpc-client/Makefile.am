VERSION_INFO = 3:0:3

include_HEADERS = \
	rcrpc-client.h

lib_LTLIBRARIES = \
	librcrpc-client.la

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = rcrpc-client.pc

librcrpc_client_la_CPPFLAGS = \
	@VISIBILITY_CFLAGS@ \
	@LIBRPC_CFLAGS@ \
	@GLIB_CFLAGS@

librcrpc_client_la_SOURCES = \
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
	rcrpc-client.c \
	rcrpc-client.h \
	sound-manager.c \
	task-manager.c \
	tuner.c \
	voip.c

nodist_librcrpc_client_la_SOURCES = \
	remote-control-stub.c \
	remote-control-stub.h

librcrpc_client_la_LDFLAGS = -version-info $(VERSION_INFO)

librcrpc_client_la_LIBADD = \
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
