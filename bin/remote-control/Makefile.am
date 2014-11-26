bin_PROGRAMS = \
	remote-control

remote_control_CPPFLAGS = \
	-DSYSCONF_DIR=\"$(sysconfdir)\" \
	-I$(top_srcdir)/src/common \
	-I$(top_srcdir)/src/server \
	-I$(top_srcdir)/bin/utils \
	@GLIB_CFLAGS@ \
	@LIBRPC_CFLAGS@ \
	@WEBKIT_CFLAGS@ \
	@GUDEV_CFLAGS@ \
	@ALSA_CFLAGS@ \
	@GTK_CFLAGS@ \
	@LIBNETTLE_CFLAGS@ \
	@LIBSOUP_CFLAGS@

remote_control_SOURCES = \
	log.c \
	log.h \
	remote-control.c \
	remote-control-data.h \
	remote-control-rdp-window.c \
	remote-control-rdp-window.h \
	remote-control-webkit-window.c \
	remote-control-webkit-window.h

remote_control_LDADD = \
	../../src/server/libremote-control.la \
	../../src/common/libcommon.la \
	../utils/libutils.la \
	@GLIB_LIBS@ \
	@LIBRPC_LIBS@ \
	@WEBKIT_LIBS@ \
	@GUDEV_LIBS@ \
	@ALSA_LIBS@ \
	@LIBNETTLE_LIBS@ \
	@LIBSOUP_LIBS@

if ENABLE_JAVASCRIPT
remote_control_SOURCES += \
	javascript.c \
	javascript.h \
	javascript-audio.c \
	javascript-backlight.c \
	javascript-cursor.c \
	javascript-fb.c \
	javascript-http-request.c \
	javascript-input.c \
	javascript-media-player.c \
	javascript-modem.c \
	javascript-monitor.c \
	javascript-output.c \
	javascript-output.h \
	javascript-output-hid-led.c \
	javascript-output-sysfs.c \
	javascript-smartcard.c \
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
if ENABLE_JAVASCRIPT_APP_WATCHDOG
remote_control_SOURCES += \
	javascript-app-watchdog.c
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

endif # ENABLE_JAVASCRIPT

EXTRA_DIST = \
	javascript-input-codes.awk \
	javascript-input-led-codes.awk \
	javascript-input-properties.awk

if ENABLE_WATCHDOG
remote_control_CPPFLAGS += @WATCHDOG_CFLAGS@
remote_control_LDADD += @WATCHDOG_LIBS@
endif
