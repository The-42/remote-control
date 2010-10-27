bin_PROGRAMS = \
	remote-control

remote_control_CFLAGS = \
	-I../../src/server \
	@LIBRPC_CFLAGS@

remote_control_SOURCES = \
	remote-control.c

remote_control_LDADD = \
	../../src/server/libremote-control.la \
	@LIBRPC_LIBS@
