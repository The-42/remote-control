noinst_LTLIBRARIES = \
	libcommon.la

libcommon_la_CPPFLAGS = \
	@GUDEV_CFLAGS@

libcommon_la_SOURCES = \
	find-device-udev.c \
	find-device.h \
	gdevicetree.c \
	gdevicetree.h \
	gkeyfile.c \
	gkeyfile.h \
	glogging.c \
	glogging.h \
	guri.c \
	guri.h

if ENABLE_BACKLIGHT_SYSFS
libcommon_la_SOURCES += \
	gsysfs.c \
	gsysfs.h \
	gsysfsbacklight.c \
	gsysfsbacklight.h
endif

if ENABLE_GPIO_SYSFS
libcommon_la_SOURCES += \
	gsysfsgpio.c \
	gsysfsgpio.h
endif

libcommon_la_LIBADD = \
	@GUDEV_LIBS@

if ENABLE_ALSALOOP
libcommon_la_CPPFLAGS += @ALSALOOP_CFLAGS@
libcommon_la_SOURCES += \
	alsaloop.c \
	rc-alsaloop.h
libcommon_la_LIBADD += @ALSALOOP_LIBS@
endif
