INCLUDES = -I ../../src/client -I $(srcdir)/../../src/client

bin_PROGRAMS = \
	remote-control-client

remote_control_client_CFLAGS = \
	-DLOCALEBASEDIR=\""$(datadir)/locale"\" \
	-DGETTEXT_PACKAGE=\"$(PACKAGE)\" \
	@READLINE_CFLAGS@ \
	@LIBSH_CFLAGS@

remote_control_client_SOURCES = \
	cli.h \
	commands.c \
	main.c

remote_control_client_LDADD = \
	../../src/client/libremote-control.la \
	@READLINE_LIBS@ \
	@LIBSH_LIBS@
