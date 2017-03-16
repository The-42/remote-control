noinst_LTLIBRARIES = \
	librcrpc.la

librcrpc_la_CFLAGS = \
	-DSYSCONF_DIR=\"$(sysconfdir)\" \
	-I$(top_srcdir)/src/server \
	@LIBNL_CFLAGS@ \
	@LIBRPC_CFLAGS@ \
	@GUDEV_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@GTK_CFLAGS@ \
	@WATCHDOG_CFLAGS@

librcrpc_la_LIBADD = \
	../../../../src/server/libremote-control.la \
	@LIBNL_LIBS@ \
	@LIBRPC_LIBS@ \
	@GUDEV_LIBS@ \
	@GLIB_LIBS@ \
	@GTK_LIBS@ \
	@WATCHDOG_LIBS@

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
	remote-control-rpc.c \
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
