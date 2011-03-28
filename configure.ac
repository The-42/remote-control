AC_PREREQ(2.66)
AC_INIT([remote-control], [0.4.7], [bugs@avionic-design.de], [remote-control])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_HEADER([config.h])
AC_CANONICAL_HOST

AM_INIT_AUTOMAKE([no-dist-gzip dist-xz foreign])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AM_MAINTAINER_MODE

AC_PROG_LIBTOOL
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL

PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.27.5)
PKG_CHECK_MODULES(WEBKIT, webkit-1.0)
PKG_CHECK_MODULES(GTK, gtk+-x11-2.0 x11 xext)
PKG_CHECK_MODULES(GLADE, libglade-2.0)
PKG_CHECK_MODULES(ALSA, alsa)

PKG_CHECK_MODULES(LIBRPC, librpc >= 0.4.6)
RPC_STUBGEN=rpc-stubgen
AC_SUBST(RPC_STUBGEN)

AC_ARG_WITH(smartcard_backend,
	[AS_HELP_STRING([--with-smartcard-backend=@<:@none/libsmartcard/libccid@:>@],
		[Use the specified SmartCard/RFID backend. [default=none]])
	], [], [with_smartcard_backend=none])

enable_libsmartcard=no
enable_libccid=no

AS_CASE([$with_smartcard_backend],
	[none], [],
	[libsmartcard], [PKG_CHECK_MODULES(LIBSMARTCARD, libsmartcard, [enable_libsmartcard=yes])],
	[libccid], [PKG_CHECK_MODULES(LIBCCID, libccid, [enable_libccid=yes])],
	[AC_MSG_ERROR([Invalid SmartCard/RFID backend: must be none, libsmartcard or libccid])]
)

AC_ARG_WITH(video_backend,
	[AS_HELP_STRING([--with-video-backend=@<:@none/libvlc/gst@:>@],
		[Use the specified video backend. [default=none]])
	], [], [with_video_backend=none])

enable_libvlc=no
enable_gst=no

#
# NOTE: for gstreamer we need pligins-base for header files and
#       interfaces for the libs. but both is in gstreamer-plugins-base
#
AS_CASE([$with_video_backend],
	[none], [],
	[libvlc], [PKG_CHECK_MODULES(LIBVLC, libvlc, [enable_libvlc=yes])],
	[gst], [PKG_CHECK_MODULES(GST, gstreamer-0.10 gstreamer-plugins-base-0.10 gstreamer-interfaces-0.10, [enable_gst=yes])],
	[AC_MSG_ERROR([Invalid video backend: must be none, libvlc or gst])]
)

AC_ARG_WITH(voip_backend,
	[AS_HELP_STRING([--with-voip-backend=@<:@none/linphone@:>@],
		[Use the specified void packend. [default=none]])
	], [], [with_voip_backend=none])

enable_linphone=no

AS_CASE([$with_voip_backend],
	[none], [],
	[linphone], [PKG_CHECK_MODULES(LINPHONE, linphone >= 3.4, [enable_linphone=yes])],
	[AC_MSG_ERROR([Invalid voip backend: must be none or linphone])]
)

AM_CONDITIONAL(ENABLE_LIBVLC, [test "x$enable_libvlc" = "xyes"])
AM_CONDITIONAL(ENABLE_GST, [test "x$enable_gst" = "xyes"])
AM_CONDITIONAL(ENABLE_LIBSMARTCARD, [test "x$enable_libsmartcard" = "xyes"])
AM_CONDITIONAL(ENABLE_LIBCCID, [test "x$enable_libccid" = "xyes"])
AM_CONDITIONAL(ENABLE_LINPHONE, [test "x$enable_linphone" = "xyes"])

AC_CHECK_HEADERS([linux/gpiodev.h])

AC_CHECK_HEADER([readline/readline.h])
AC_CHECK_LIB([readline], [readline], [use_readline=yes], [use_readline=no])
if test "x$use_readline" = "xyes"; then
	READLINE_CFLAGS="-DUSE_READLINE"
	READLINE_LIBS="-lreadline"
fi
AC_SUBST(READLINE_CFLAGS)
AC_SUBST(READLINE_LIBS)

AX_CFLAGS_GCC_OPTION(-fvisibility=hidden, VISIBILITY_CFLAGS)
AC_SUBST(VISIBILITY_CFLAGS)

CFLAGS="$CFLAGS -Wall"

AC_ARG_ENABLE([werror],
	[AS_HELP_STRING([--enable-werror],
		[Treat warnings as errors (default: enabled)])],
		[enable_werror="$enableval"],
		[enable_werror=yes])
if test "x$enable_werror" = "xyes"; then
	CFLAGS="$CFLAGS -Werror"
fi

COMMANDS_ALIGN=32
AC_DEFINE_UNQUOTED([COMMANDS_ALIGN], [$COMMANDS_ALIGN],
	  [Define to the number of bytes to align commands to.])
AC_SUBST(COMMANDS_ALIGN)

AC_OUTPUT([
	bin/Makefile
	bin/remote-control-client/Makefile
	bin/remote-control-client/remote-control-client.lds
	bin/remote-control-gtk/Makefile
	bin/remote-control/Makefile
	data/de.avionic-design.RemoteControl.service
	data/Makefile
	Makefile
	src/client/Makefile
	src/client/remote-control.pc
	src/Makefile
	src/server/Makefile
])

echo "
remote-control was configured with the following options:

Features:
  readline support:          $use_readline
  Video Backend:             $with_video_backend
  SmartCard/RFID Backend:    $with_smartcard_backend
  VoIP Backend:              $with_voip_backend
"
