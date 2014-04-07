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
	@GTK_CFLAGS@

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
	@ALSA_LIBS@

if ENABLE_JAVASCRIPT
remote_control_SOURCES += \
	javascript.c \
	javascript.h \
	javascript-cursor.c \
	javascript-input.c \
	javascript-monitor.c \
	javascript-taskmanager.c

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
endif # ENABLE_JAVASCRIPT

if ENABLE_WATCHDOG
remote_control_CPPFLAGS += @WATCHDOG_CFLAGS@
remote_control_LDADD += @WATCHDOG_LIBS@
endif
