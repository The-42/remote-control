noinst_LTLIBRARIES = \
	libremote-control.la

libremote_control_la_CFLAGS = \
	@GLIB_GIO_CFLAGS@ \
	@LINPHONE_CFLAGS@ \
	@LIBVLC_CFLAGS@ \
	@GTK_CFLAGS@

libremote_control_la_SOURCES = \
	audio.c \
	backlight.c \
	card.c \
	io.c \
	irq.c \
	lldp.c \
	media.c \
	media-player.c \
	media-player-gtk-vlc.c \
	modem.c \
	net.c \
	remote-control.c \
	remote-control-stub.c \
	sound.c \
	system.c \
	tuner.c \
	voip.c \
	voip-linphone.c

libremote_control_la_LIBADD = \
	@LINPHONE_LIBS@ \
	@LIBVLC_LIBS@ \
	@GTK_LIBS@

remote-control-stub.c: $(srcdir)/../protocol.xml
	$(AM_V_GEN)$(RPC_STUBGEN) --server --source -o $@ $<

remote-control-stub.h: $(srcdir)/../protocol.xml
	$(AM_V_GEN)$(RPC_STUBGEN) --server --header -o $@ $<

BUILT_SOURCES = \
	remote-control-stub.c \
	remote-control-stub.h

CLEANFILES = \
	$(BUILT_SOURCES)
