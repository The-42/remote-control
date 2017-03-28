VERSION_INFO = 3:0:3

include_HEADERS = \
	remote-control-rpc.h

lib_LTLIBRARIES = \
	librcrpc.la

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = rcrpc.pc

librcrpc_la_CFLAGS = \
	-DSYSCONF_DIR=\"$(sysconfdir)\" \
	@REMOTE_CONTROL_CFLAGS@ \
	@LIBRPC_CFLAGS@ \
	@GLIB_CFLAGS@

librcrpc_la_LDFLAGS = -version-info $(VERSION_INFO)

librcrpc_la_LIBADD = \
	@REMOTE_CONTROL_LIBS@ \
	@LIBRPC_LIBS@ \
	@GLIB_LIBS@

librcrpc_la_SOURCES = \
	audio.c \
	backlight.c \
	handset.c \
	io.c \
	irq.c \
	lldp.c \
	media.c \
	media-player.c \
	mixer.c \
	modem.c \
	net.c \
	rcrpc.c \
	rcrpc.h \
	remote-control-rpc.h \
	smartcard.c \
	sound.c \
	system.c \
	task-manager.c \
	tuner.c \
	voip.c

nodist_librcrpc_la_SOURCES = \
	remote-control-stub.c \
	remote-control-stub.h

remote-control-stub.c: $(srcdir)/../protocol.xml
	$(AM_V_GEN)$(RPC_STUBGEN) --server --source -o $@ $<

remote-control-stub.h: $(srcdir)/../protocol.xml
	$(AM_V_GEN)$(RPC_STUBGEN) --server --header -o $@ $<

BUILT_SOURCES = \
	remote-control-stub.c \
	remote-control-stub.h

CLEANFILES = \
	$(BUILT_SOURCES)
