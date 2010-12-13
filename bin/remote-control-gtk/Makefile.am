INCLUDES = -I ../../src/client -I $(srcdir)/../../src/client

bin_PROGRAMS = \
	remote-control-gtk

remote_control_gtk_SOURCES = \
	audio.c \
	backlight.c \
	card.c \
	main.c \
	mediaplayer.c \
	miscellaneous.c \
	modem.c \
	tuner.c \
	voip.c \
	webkit.c

remote_control_gtk_CFLAGS = \
	@LIBRPC_CFLAGS@ \
	@WEBKIT_CFLAGS@ \
	@GLADE_CFLAGS@ \
	@GTK_CFLAGS@

remote_control_gtk_LDFLAGS = \
	-Wl,--export-dynamic

remote_control_gtk_LDADD = \
	../../src/client/libremote-control.la \
	@WEBKIT_LIBS@ \
	@GLADE_LIBS@ \
	@GTK_LIBS@
