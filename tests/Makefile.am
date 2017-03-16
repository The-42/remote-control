noinst_PROGRAMS = \
	ajax-dead-lock \
	alert-dead-lock \
	gkeyfilemerge \
	net-udp

ajax_dead_lock_CFLAGS = @WEBKIT_CFLAGS@
ajax_dead_lock_SOURCES = ajax-dead-lock.c
ajax_dead_lock_LDADD = @WEBKIT_LIBS@

alert_dead_lock_CFLAGS = @WEBKIT_CFLAGS@
alert_dead_lock_SOURCES = alert-dead-lock.c
alert_dead_lock_LDADD = @WEBKIT_LIBS@

gkeyfilemerge_CFLAGS = -I$(top_srcdir)/src/common @GLIB_CFLAGS@
gkeyfilemerge_SOURCES = gkeyfilemerge.c
gkeyfilemerge_LDADD = @GLIB_LIBS@ ../src/common/libcommon.la

net_udp_CFLAGS = @WEBKIT_CFLAGS@ -I$(top_srcdir)/src/server
net_udp_SOURCES = net-udp.c
net_udp_LDADD = @GLIB_LIBS@ ../src/server/libremote-control.la
