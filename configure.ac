AC_PREREQ(2.66)
AC_INIT([remote-control], [0.2.1], [bugs@avionic-design.de], [remote-control])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_HEADER([config.h])
AC_CANONICAL_HOST

AM_INIT_AUTOMAKE([dist-bzip2 dist-xz foreign])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AM_MAINTAINER_MODE

AC_PROG_LIBTOOL
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL

PKG_CHECK_MODULES(GLIB_GIO, glib-2.0 gio-2.0)
PKG_CHECK_MODULES(LINPHONE, linphone >= 3.3.99.10)
PKG_CHECK_MODULES(WEBKIT, webkit-1.0)
PKG_CHECK_MODULES(GTK, gtk+-2.0)
PKG_CHECK_MODULES(GLADE, libglade-2.0)
PKG_CHECK_MODULES(LIBSH, libsh)
PKG_CHECK_MODULES(ALSA, alsa)

PKG_CHECK_MODULES(LIBRPC, librpc >= 0.4.2)
RPC_STUBGEN=rpc-stubgen
AC_SUBST(RPC_STUBGEN)

PKG_CHECK_MODULES(LIBSMARTCARD, libsmartcard,
	[have_libsmartcard=yes],
	[have_libsmartcard=no])
AM_CONDITIONAL(HAVE_LIBSMARTCARD, [test "x$have_libsmartcard" = "xyes"])

AC_DEFINE(ac_video_backend, ["none"], ["Video Backend to use. Possible values are: none, libvlc, gstreamer"])
AC_ARG_WITH(video_backend,[AS_HELP_STRING([--with-video-backend],
	[Use the specified backend (libvlc|gstreamer) for video playback. [default=none] ])
	],[ ac_video_backend="$withval" ],[ ac_video_backend="" ])

AS_IF([test "x$ac_video_backend" = "xlibvlc"], [
	PKG_CHECK_MODULES(LIBVLC, libvlc)
	enable_libvlc=yes
], [
	enable_libvlc=no
])
AM_CONDITIONAL(ENABLE_LIBVLC, [test "x$enable_libvlc" = "xyes"])

AS_IF([test "x$ac_video_backend" = "xgstreamer"], [
	PKG_CHECK_MODULES(GSTREAMER, gstreamer-0.10)
	enable_gstreamer=yes
],[
	enable_gstreamer=no
])
AM_CONDITIONAL(ENABLE_GSTREAMER, [test "x$enable_gstreamer" = "xyes"])

AC_CHECK_HEADERS([linux/gpiodev.h])

AC_CHECK_HEADER([readline/readline.h])
AC_CHECK_LIB([readline], [readline], [use_readline=yes], [use_readline=no])
if test "x$use_readline" = "xyes"; then
	READLINE_CFLAGS="-DUSE_READLINE"
	READLINE_LIBS="-lreadline"
fi
AC_SUBST(READLINE_CFLAGS)
AC_SUBST(READLINE_LIBS)

CFLAGS="$CFLAGS -Wall"

AC_ARG_ENABLE([werror],
	[AS_HELP_STRING([--enable-werror],
		[Treat warnings as errors (default: enabled)])],
		[enable_werror="$enableval"],
		[enable_werror=yes])
if test "x$enable_werror" = "xyes"; then
	CFLAGS="$CFLAGS -Werror"
fi

AC_OUTPUT([
	bin/Makefile
	bin/remote-control-client/Makefile
	bin/remote-control-gtk/Makefile
	bin/remote-control/Makefile
	data/de.avionic-design.RemoteControl.service
	data/Makefile
	Makefile
	src/client/Makefile
	src/Makefile
	src/server/Makefile
])
