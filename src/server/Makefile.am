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
	event-manager.c \
	io.c \
	irq.c \
	lldp.c \
	lldp-monitor.c \
	media.c \
	media-player.c \
	media-player-gtk-vlc.c \
	mixer.c \
	mixer-alsa.c \
	modem.c \
	net.c \
	net-unix.c \
	remote-control.c \
	remote-control.h \
	smartcard.c \
	sound.c \
	system.c \
	tuner.c \
	utils.c \
	voip.c \
	voip-linphone.c

nodist_libremote_control_la_SOURCES = \
	remote-control-stub.c \
	remote-control-stub.h

libremote_control_la_LIBADD = \
	@LINPHONE_LIBS@ \
	@LIBVLC_LIBS@ \
	@GTK_LIBS@

if HAVE_LIBSMARTCARD
libremote_control_la_CFLAGS += \
	@LIBSMARTCARD_CFLAGS@

libremote_control_la_LIBADD += \
	@LIBSMARTCARD_LIBS@

libremote_control_la_SOURCES += \
	smartcard-i2c.c
else
libremote_control_la_SOURCES += \
	smartcard-null.c
endif

remote-control-stub.c: $(srcdir)/../protocol.xml
	$(AM_V_GEN)$(RPC_STUBGEN) --server --source -o $@ $<

remote-control-stub.h: $(srcdir)/../protocol.xml
	$(AM_V_GEN)$(RPC_STUBGEN) --server --header -o $@ $<

BUILT_SOURCES = \
	remote-control-stub.c \
	remote-control-stub.h

CLEANFILES = \
	$(BUILT_SOURCES)
