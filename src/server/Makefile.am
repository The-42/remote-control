noinst_LTLIBRARIES = \
	libremote-control.la

libremote_control_la_SOURCES = \
	audio.c \
	backlight.c \
	card.c \
	io.c \
	irq.c \
	lldp.c \
	media.c \
	media-player.c \
	modem.c \
	net.c \
	remote-control.c \
	sound.c \
	system.c \
	tuner.c \
	voip.c

remote-control.c: $(srcdir)/../protocol.xml
	$(AM_V_GEN)$(RPC_STUBGEN) --server --source -o $@ $<

remote-control.h: $(srcdir)/../protocol.xml
	$(AM_V_GEN)$(RPC_STUBGEN) --server --header -o $@ $<

BUILT_SOURCES = \
	remote-control.c \
	remote-control.h

CLEANFILES = \
	$(BUILT_SOURCES)
