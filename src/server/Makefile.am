noinst_LTLIBRARIES = \
	libremote-control.la

libremote_control_la_CFLAGS = \
	-DSYSCONF_DIR=\"$(sysconfdir)\" \
	-I$(top_srcdir)/src/common \
	@LIBNL_CFLAGS@ \
	@LIBRPC_CFLAGS@ \
	@GUDEV_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@GTK_CFLAGS@ \
	@WATCHDOG_CFLAGS@

libremote_control_la_LIBADD = \
	../common/libcommon.la \
	@LIBNL_LIBS@ \
	@LIBRPC_LIBS@ \
	@GUDEV_LIBS@ \
	@GLIB_LIBS@ \
	@GTK_LIBS@ \
	@WATCHDOG_LIBS@

libremote_control_la_SOURCES = \
	audio.c \
	backlight.c \
	cursor-movement.c \
	event-manager.c \
	handset.c \
	io.c \
	irq.c \
	lldp.c \
	lldp-monitor.c \
	media.c \
	media-player.c \
	mixer.c \
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
	tuner-v4l2.c \
	usb-handset.c \
	utils.c \
	voip.c

nodist_libremote_control_la_SOURCES = \
	remote-control-stub.c \
	remote-control-stub.h

if ENABLE_AUDIO_ALSA
libremote_control_la_CFLAGS += @ALSA_CFLAGS@
libremote_control_la_LIBADD += @ALSA_LIBS@
libremote_control_la_SOURCES += audio-alsa.c
else
if ENABLE_AUDIO_PULSE
libremote_control_la_CFLAGS += @PULSE_CFLAGS@
libremote_control_la_LIBADD += @PULSE_LIBS@
libremote_control_la_SOURCES += audio-pulse.c
else
libremote_control_la_SOURCES += audio-null.c
endif # ENABLE_AUDIO_PULSE
endif #ENABLE_AUDIO_ALSA

if ENABLE_MIXER_ALSA
libremote_control_la_CFLAGS += @ALSA_CFLAGS@
libremote_control_la_SOURCES += mixer-alsa.c
libremote_control_la_LIBADD += @ALSA_LIBS@
else
libremote_control_la_SOURCES += mixer-null.c
endif

if ENABLE_BACKLIGHT_DPMS
libremote_control_la_SOURCES += backlight-dpms.c
else
if ENABLE_BACKLIGHT_MEDATOM
libremote_control_la_SOURCES += backlight-medatom.c smbus.h
else
if ENABLE_BACKLIGHT_SYSFS
libremote_control_la_SOURCES += backlight-sysfs.c
else
libremote_control_la_SOURCES += backlight-null.c
endif # ENABLE_BACKLIGHT_SYSFS
endif # ENABLE_BACKLIGHT_MEDATOM
endif # ENABLE_BACKLIGHT_DPMS

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
	@GST_CFLAGS@ \
	@XRANDR_CFLAGS@

libremote_control_la_LIBADD += \
	@GST_LIBS@ \
	@XRANDR_LIBS@

libremote_control_la_SOURCES += \
	media-player-gtk-gst.c \
	sound-manager-gst.c
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

if ENABLE_LIBPTIP
libremote_control_la_CFLAGS += @LIBPTIP_CFLAGS@
libremote_control_la_LIBADD += @LIBPTIP_LIBS@
libremote_control_la_SOURCES += handset-ptip.c
else
libremote_control_la_SOURCES += handset-null.c
endif # ENABLE_LIBPTIP

if ENABLE_GPIO_GPIODEV
libremote_control_la_SOURCES += gpio-gpiodev.c
else
if ENABLE_GPIO_SYSFS
libremote_control_la_CFLAGS += @LIBGPIO_CFLAGS@
libremote_control_la_LIBADD += @LIBGPIO_LIBS@
libremote_control_la_SOURCES += gpio-sysfs.c
else
libremote_control_la_SOURCES += gpio-null.c
endif # ENABLE_GPIO_SYSFS
endif # ENABLE_GPIO_GPIODEV

if ENABLE_WATCHDOG
libremote_control_la_SOURCES += app-watchdog-dbus.c
else
libremote_control_la_SOURCES += app-watchdog-null.c
endif # ENABLE_WATCHDOG

remote-control-stub.c: $(srcdir)/../protocol.xml
	$(AM_V_GEN)$(RPC_STUBGEN) --server --source -o $@ $<

remote-control-stub.h: $(srcdir)/../protocol.xml
	$(AM_V_GEN)$(RPC_STUBGEN) --server --header -o $@ $<

libremote_control_la_SOURCES += gtkosk-dbus.c

gtkosk-dbus.c: gtkosk-dbus.h
	touch $@

gtkosk-dbus.h:
	gdbus-codegen \
		--generate-c-code gtkosk-dbus \
		--c-namespace GtkOsk \
		--interface-prefix com.avionicdesign.gtkosk. \
		@GTKOSK_DATADIR@/com.avionicdesign.gtkosk.control.xml

BUILT_SOURCES = \
	remote-control-stub.c \
	remote-control-stub.h \
	gtkosk-dbus.c \
	gtkosk-dbus.h

if ENABLE_LIBTNJ3324
libremote_control_la_CFLAGS += @LIBTNJ3324_CFLAGS@
libremote_control_la_LIBADD += @LIBTNJ3324_LIBS@
endif

CLEANFILES = \
	$(BUILT_SOURCES)
