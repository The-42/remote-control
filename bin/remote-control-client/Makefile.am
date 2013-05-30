bin_PROGRAMS = \
	remote-control-client

remote_control_client_CFLAGS = \
	-I ../../src/client \
	-I $(srcdir)/../../src/client \
	-DLOCALEBASEDIR=\""$(datadir)/locale"\" \
	-DGETTEXT_PACKAGE=\"$(PACKAGE)\" \
	@READLINE_CFLAGS@ \
	@LIBRPC_CFLAGS@

remote_control_client_SOURCES = \
	remote-control-client.h \
	commands.c \
	main.c \
	utils.c

remote_control_client_LDADD = \
	../../src/client/libremote-control.la \
	remote-control-client.lds \
	@READLINE_LIBS@ \
	@LIBRPC_LIBS@

BUILT_SOURCES = \
	remote-control-client.lds
