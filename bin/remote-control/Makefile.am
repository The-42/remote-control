bin_PROGRAMS = \
	remote-control

remote_control_CPPFLAGS = \
	-DSYSCONF_DIR=\"$(sysconfdir)\" \
	-I$(top_srcdir)/src/common \
	-I$(top_srcdir)/src/core \
	-I$(top_srcdir)/bin/utils \
	@GLIB_CFLAGS@ \
	@X11_CFLAGS@ \
	@WEBKIT_CFLAGS@ \
	@GUDEV_CFLAGS@ \
	@ALSA_CFLAGS@ \
	@LIBNETTLE_CFLAGS@ \
	@LIBSOUP_CFLAGS@

remote_control_SOURCES = \
	extensions.h \
	log.c \
	log.h \
	remote-control.c \
	remote-control-data.h \
	remote-control-rdp-window.c \
	remote-control-rdp-window.h \
	remote-control-webkit-window.c \
	remote-control-webkit-window.h

remote_control_LDADD = \
	../../src/core/libremote-control.la \
	../../src/common/libcommon.la \
	../utils/libutils.la \
	@GLIB_LIBS@ \
	@X11_LIBS@ \
	@WEBKIT_LIBS@ \
	@GUDEV_LIBS@ \
	@ALSA_LIBS@ \
	@LIBNETTLE_LIBS@ \
	@LIBSOUP_LIBS@

remote_control_SOURCES += \
	javascript.c \
	javascript.h \
	javascript-audio.c \
	javascript-backlight.c \
	javascript-cursor.c \
	javascript-event-manager.c \
	javascript-fb.c \
	javascript-http-request.c \
	javascript-input.c \
	javascript-lldp.c \
	javascript-media-player.c \
	javascript-output.c \
	javascript-output.h \
	javascript-output-hid-led.c \
	javascript-output-sysfs.c \
	javascript-smartcard.c \
	javascript-sysinfo.c \
	javascript-taskmanager.c \
	javascript-voip.c

if ENABLE_JAVASCRIPT_IR
remote_control_SOURCES += \
	javascript-irkey.c
endif  # ENABLE_JAVASCRIPT_IR
if ENABLE_JAVASCRIPT_LCD
remote_control_SOURCES += \
	javascript-lcd.c
endif  # ENABLE_JAVASCRIPT_LCD
if ENABLE_JAVASCRIPT_MEDIAL
remote_control_SOURCES += \
	javascript-medial.c
endif  # ENABLE_JAVASCRIPT_MEDIAL
if ENABLE_JAVASCRIPT_APP_WATCHDOG
remote_control_SOURCES += \
	javascript-app-watchdog.c
endif
if ENABLE_LIBMODEM
remote_control_SOURCES += \
	javascript-modem.c
endif  # ENABLE_LIBMODEM

if ENABLE_EXTENSIONS
remote_control_SOURCES += \
	extensions.c
endif

# javascript-input.c needs some generated tables
$(srcdir)/javascript-input.c: \
	javascript-input-codes.c \
	javascript-input-properties.c

$(srcdir)/javascript-output-hid-led.c: \
	javascript-input-led-codes.c

# Find the path for linux/input.h
INPUT_DEPS = $(shell printf "\x23include <linux/input.h>\n" | \
		$(CC) $(CFLAGS) -E -M -)
# But fallback on /dev/null if we couldn't find linux/input.h
INPUT_H = $(or $(filter %/linux/input.h,$(INPUT_DEPS)),/dev/null)

# Rule to generate the tables from linux/input.h
javascript-input-%.c: $(srcdir)/javascript-input-%.awk
	$(AM_V_GEN)awk -f $< $(INPUT_H) > $@ || (rm -f $@ ; false)

EXTRA_DIST = \
	javascript-input-codes.awk \
	javascript-input-led-codes.awk \
	javascript-input-properties.awk

CLEANFILES = \
	$(patsubst %.awk,%.c,$(EXTRA_DIST))

if ENABLE_WATCHDOG
remote_control_CPPFLAGS += @WATCHDOG_CFLAGS@
remote_control_LDADD += @WATCHDOG_LIBS@
endif
