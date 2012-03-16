noinst_LTLIBRARIES = \
	libutils.la

libutils_la_CPPFLAGS = \
	@WATCHDOG_CFLAGS@ \
	@WEBKIT_CFLAGS@

libutils_la_SOURCES = \
	utils.h \
	watchdog.c \
	webkit.c

libutils_la_LIBADD = \
	@WATCHDOG_LIBS@ \
	@WEBKIT_LIBS@
