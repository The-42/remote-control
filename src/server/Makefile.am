noinst_LTLIBRARIES = \
	libremote-control.la

libremote_control_la_CFLAGS = \
	-I$(top_srcdir)/src/common \
	@LIBNL_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@GTK_CFLAGS@

libremote_control_la_LIBADD = \
	../common/libcommon.la \
	@LIBNL_LIBS@ -lnl-route \
	@GLIB_LIBS@ \
	@GTK_LIBS@

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
	mixer.c \
	mixer-alsa.c \
	modem.c \
	net.c \
	net-unix.c \
	remote-control.c \
	remote-control.h \
	rfid.c \
	smartcard.c \
	sound.c \
	system.c \
	task-manager.c \
	tuner.c \
	utils.c \
	voip.c

nodist_libremote_control_la_SOURCES = \
	remote-control-stub.c \
	remote-control-stub.h

if ENABLE_DPMS
libremote_control_la_SOURCES += backlight-dpms.c
else
if ENABLE_MEDATOM_BACKLIGHT
libremote_control_la_CPPFLAGS += @GUDEV_CFLAGS@
libremote_control_la_SOURCES += backlight-medatom.c smbus.h
libremote_control_la_LIBADD += @GUDEV_LIBS@
else
libremote_control_la_SOURCES += backlight-null.c
endif # ENABLE_MEDATOM_BACKLIGHT
endif # ENABLE_DPMS

if ENABLE_LINPHONE
libremote_control_la_CFLAGS += \
	@LINPHONE_CFLAGS@

libremote_control_la_LIBADD += \
	@LINPHONE_LIBS@

libremote_control_la_SOURCES += \
	voip-linphone.c
else
libremote_control_la_SOURCES += \
	voip-null.c
endif # ENABLE_LINPHONE

if ENABLE_LIBVLC
libremote_control_la_CFLAGS += \
	@LIBVLC_CFLAGS@

libremote_control_la_LIBADD += \
	@LIBVLC_LIBS@

libremote_control_la_SOURCES += \
	media-player-gtk-vlc.c \
	sound-manager-vlc.c
else
if ENABLE_GST
libremote_control_la_CFLAGS += \
	@GST_CFLAGS@

libremote_control_la_LIBADD += \
	@GST_LIBS@

libremote_control_la_SOURCES += \
	media-player-gtk-gst.c \
	sound-manager-null.c
else
libremote_control_la_SOURCES += \
	media-player-null.c \
	sound-manager-null.c
endif # ENABLE_GST
endif # ENABLE_LIBVLC

if ENABLE_LIBCCID
libremote_control_la_CFLAGS += @LIBCCID_CFLAGS@
libremote_control_la_LIBADD += @LIBCCID_LIBS@
libremote_control_la_SOURCES += smartcard-libccid.c
else
if ENABLE_LIBSMARTCARD
libremote_control_la_CFLAGS += @LIBSMARTCARD_CFLAGS@
libremote_control_la_LIBADD += @LIBSMARTCARD_LIBS@
libremote_control_la_SOURCES += smartcard-i2c.c
else
libremote_control_la_SOURCES += smartcard-null.c
endif # ENABLE_LIBSMARTCARD
libremote_control_la_SOURCES += rfid-null.c
endif # ENABLE_LIBCCID

if ENABLE_LIBMODEM
libremote_control_la_CFLAGS += @LIBMODEM_CFLAGS@
libremote_control_la_LIBADD += @LIBMODEM_LIBS@
libremote_control_la_SOURCES += modem-manager-libmodem.c
else
libremote_control_la_SOURCES += modem-manager-null.c
endif # ENABLE_LIBMODEM

remote-control-stub.c: $(srcdir)/../protocol.xml
	$(AM_V_GEN)$(RPC_STUBGEN) --server --source -o $@ $<

remote-control-stub.h: $(srcdir)/../protocol.xml
	$(AM_V_GEN)$(RPC_STUBGEN) --server --header -o $@ $<

BUILT_SOURCES = \
	remote-control-stub.c \
	remote-control-stub.h

CLEANFILES = \
	$(BUILT_SOURCES)
