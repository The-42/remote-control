AC_PREREQ(2.66)
AC_INIT([remote-control], [0.6.3], [bugs@avionic-design.de], [remote-control])
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

PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.27.5 gio-2.0)
PKG_CHECK_MODULES(GLADE, libglade-2.0)
PKG_CHECK_MODULES(GTKOSK, gtkosk)
PKG_CHECK_MODULES(ALSA, alsa)
PKG_CHECK_MODULES(LIBNL, libnl-route-3.0)
PKG_CHECK_MODULES(POPPLER, poppler-glib)
PKG_CHECK_MODULES(GUDEV, gudev-1.0)

PKG_CHECK_MODULES(LIBRPC, librpc >= 0.4.10)
RPC_STUBGEN=rpc-stubgen
AC_SUBST(RPC_STUBGEN)

#
# Gtk+ version
#
AC_ARG_WITH([gtk],
	[AS_HELP_STRING([--with-gtk=@<:@2.0|3.0@:>@],
			[The Gtk+ version to use (default: 2.0)])
	], [], [with_gtk=2.0])

AS_CASE([$with_gtk],
	[2.0], [REQUIRED_WEBKIT=-1.0],
	[3.0], [REQUIRED_WEBKIT=gtk-3.0],
	[AC_MSG_ERROR([Invalid Gtk+ version specified: must be 2.0 or 3.0])])

if test "x$with_gtk" = "x3.0"; then
	AC_DEFINE([HAVE_GTKX_H], [1], [If Gtk+ 3.0 is used])
fi

PKG_CHECK_MODULES(GTK, gtk+-x11-$with_gtk x11 xext)
PKG_CHECK_MODULES(WEBKIT, webkit$REQUIRED_WEBKIT)

#
# audio (UCM) backend
#
enable_audio_alsa=no

AC_ARG_WITH(audio_backend,
	[AS_HELP_STRING([--with-audio-backend=@<:@none/alsa@:>@],
		[Use the specified audio (UCM) backend. [default=none]])
	], [], [with_audio_backend=none])

AS_CASE([$with_audio_backend],
	[none], [],
	[alsa], [enable_audio_alsa=yes],
	[AC_MSG_ERROR([Invalid audio backend: must be none or alsa])]
)

AS_IF([test "x$enable_audio_alsa" = "xyes"], [PKG_CHECK_MODULES(ALSA, alsa)])

#
# backlight backend
#
enable_backlight_dpms=no
enable_backlight_medatom=no
enable_backlight_sysfs=no

AC_ARG_WITH(backlight_backend,
	[AS_HELP_STRING([--with-backlight-backend=@<:@none/dpms/medatom/sysfs@:>@],
		[Use the specified backlight backend. [default=none]])
	], [], [with_backlight_backend=none])

AS_CASE([$with_backlight_backend],
	[none], [],
	[dpms], [enable_backlight_dpms=yes],
	[medatom], [enable_backlight_medatom=yes],
	[sysfs], [enable_backlight_sysfs=yes],
	[AC_MSG_ERROR([Invalid backlight backend: must be none, dpms, medatom or sysfs])]
)

AS_IF([test "x$enable_backlight_medatom" = "xyes"], [PKG_CHECK_MODULES(GUDEV, gudev-1.0)])

#
# SmartCard backend
#
enable_libsmartcard=no
enable_libccid=no

AC_ARG_WITH(smartcard_backend,
	[AS_HELP_STRING([--with-smartcard-backend=@<:@none/libsmartcard/libccid@:>@],
		[Use the specified SmartCard/RFID backend. [default=none]])
	], [], [with_smartcard_backend=none])

AS_CASE([$with_smartcard_backend],
	[none], [],
	[libsmartcard], [PKG_CHECK_MODULES(LIBSMARTCARD, libsmartcard, [enable_libsmartcard=yes])],
	[libccid], [PKG_CHECK_MODULES(LIBCCID, libccid, [enable_libccid=yes])],
	[AC_MSG_ERROR([Invalid SmartCard/RFID backend: must be none, libsmartcard or libccid])]
)

#
# Video backend
#
enable_libvlc=no
enable_gst=no

AC_ARG_WITH(video_backend,
	[AS_HELP_STRING([--with-video-backend=@<:@none/libvlc/gst@:>@],
		[Use the specified video backend. [default=none]])
	], [], [with_video_backend=none])

#
# NOTE: for gstreamer we need plugins-base for header files and
#       interfaces for the libs. but both is in gstreamer-plugins-base
#
AS_CASE([$with_video_backend],
	[none], [],
	[libvlc], [PKG_CHECK_MODULES(LIBVLC, libvlc, [enable_libvlc=yes])],
	[gst], [PKG_CHECK_MODULES(GST, gstreamer-0.10 gstreamer-plugins-base-0.10 gstreamer-interfaces-0.10, [enable_gst=yes])],
	[AC_MSG_ERROR([Invalid video backend: must be none, libvlc or gst])]
)
dnl FIXME: This is not the way to go, try to do this in AC_CASE[gst]
if test "x$enable_gst" = "xyes"; then
	PKG_CHECK_MODULES(XRANDR, xrandr >= 1.3 xrender x11 xproto >= 7.0.17)
fi

#
# VoIP backend
#
enable_linphone=no

AC_ARG_WITH(voip_backend,
	[AS_HELP_STRING([--with-voip-backend=@<:@none/linphone@:>@],
		[Use the specified VoIP backend. [default=none]])
	], [], [with_voip_backend=none])

AS_CASE([$with_voip_backend],
	[none], [],
	[linphone], [PKG_CHECK_MODULES(LINPHONE, linphone >= 3.4, [enable_linphone=yes])],
	[AC_MSG_ERROR([Invalid voip backend: must be none or linphone])]
)

#
# Modem backend
#
enable_libmodem=no

AC_ARG_WITH(modem_backend,
	[AS_HELP_STRING([--with-modem-backend=@<:@none/libmodem@:>@],
		[Use the specified modem backend. [default=none]])
	], [], [with_modem_backend=none])

AS_CASE([$with_modem_backend],
	[none], [],
	[libmodem], [PKG_CHECK_MODULES(LIBMODEM, libmodem >= 0.2.1, [enable_libmodem=yes])],
	[AC_MSG_ERROR([Invalid modem backend: must be none or libmodem])]
)

#
# Handset backend
#
enable_libptip=no

AC_ARG_WITH(handset_backend,
	[AS_HELP_STRING([--with-handset-backend=@<:@none/ptip@:>@],
		[Use the specified handset backend. [default=none]])
	], [], [with_handset_backend=none])

AS_CASE([$with_handset_backend],
	[none], [],
	[ptip], [PKG_CHECK_MODULES(LIBPTIP, libptip, [enable_libptip=yes])],
	[AC_MSG_ERROR([Invalid handset backend: must be none or ptip])]
)

#
# GPIO backend
#
enable_gpio_gpiodev=no
enable_gpio_sysfs=no

AC_ARG_WITH(gpio_backend,
	[AS_HELP_STRING([--with-gpio-backend=@<:@none/gpiodev/sysfs@:>@],
		[Use the specified GPIO backend. [default=none]])
	], [], [with_gpio_backend=none])

AS_CASE([$with_gpio_backend],
	[none], [],
	[gpiodev], [enable_gpio_gpiodev=yes],
	[sysfs], [enable_gpio_sysfs=yes],
	[AC_MSG_ERROR([Invalid GPIO backend: must be none, gpiodev or sysfs])]
)

#
# JavaScript binding
#
AC_MSG_CHECKING([whether to enable JavaScript binding])
AC_ARG_ENABLE([javascript],
	[AS_HELP_STRING([--enable-javascript],
			[Enable JavaScript binding [default=no]])],
	[], [enable_javascript=no])
AC_MSG_RESULT([$enable_javascript])

AS_IF([test "x$enable_javascript" = "xyes"], [
	AC_DEFINE([ENABLE_JAVASCRIPT], [1], [whether JavaScript binding is enabled])

	AC_MSG_CHECKING([whether to enable JavaScript IR input support])
	AC_ARG_ENABLE(javascript-ir,
		[AS_HELP_STRING([--enable-javascript-ir],
				[Enable JavaScript IR input support (default: no)])],
		[], [enable_javascript_ir=no])
	AC_MSG_RESULT([$enable_javascript_ir])

	AS_IF([test "x$enable_javascript_ir" = "xyes"], [
		AC_DEFINE([ENABLE_JAVASCRIPT_IR], [1],
			  [whether JavaScript IR input support is enabled])
	])
])

#
# define automake conditionals according to select backends
#
AM_CONDITIONAL(ENABLE_AUDIO_ALSA, [test "x$enable_audio_alsa" = "xyes"])
AM_CONDITIONAL(ENABLE_BACKLIGHT_DPMS, [test "x$enable_backlight_dpms" = "xyes"])
AM_CONDITIONAL(ENABLE_BACKLIGHT_MEDATOM, [test "x$enable_backlight_medatom" = "xyes"])
AM_CONDITIONAL(ENABLE_BACKLIGHT_SYSFS, [test "x$enable_backlight_sysfs" = "xyes"])
AM_CONDITIONAL(ENABLE_LIBVLC, [test "x$enable_libvlc" = "xyes"])
AM_CONDITIONAL(ENABLE_GST, [test "x$enable_gst" = "xyes"])
AM_CONDITIONAL(ENABLE_LIBSMARTCARD, [test "x$enable_libsmartcard" = "xyes"])
AM_CONDITIONAL(ENABLE_LIBCCID, [test "x$enable_libccid" = "xyes"])
AM_CONDITIONAL(ENABLE_LINPHONE, [test "x$enable_linphone" = "xyes"])
AM_CONDITIONAL(ENABLE_LIBMODEM, [test "x$enable_libmodem" = "xyes"])
AM_CONDITIONAL(ENABLE_LIBPTIP, [test "x$enable_libptip" = "xyes"])
AM_CONDITIONAL(ENABLE_GPIO_GPIODEV, [test "x$enable_gpio_gpiodev" = "xyes"])
AM_CONDITIONAL(ENABLE_GPIO_SYSFS, [test "x$enable_gpio_sysfs" = "xyes"])
AM_CONDITIONAL(ENABLE_JAVASCRIPT, [test "x$enable_javascript" = "xyes"])
AM_CONDITIONAL(ENABLE_JAVASCRIPT_IR, [test "x$enable_javascript_ir" = "xyes"])

#
# check for required files and packages
#
AS_IF([test "x$enable_gpio_gpiodev" = "xyes"],
	[AC_CHECK_HEADERS([linux/gpiodev.h], [],
		[AC_MSG_ERROR([linux/gpiodev.h is required for gpiodev backend])]
	)])

AC_CHECK_HEADER([readline/readline.h])
AC_CHECK_LIB([readline], [readline], [use_readline=yes], [use_readline=no])
if test "x$use_readline" = "xyes"; then
	READLINE_CFLAGS="-DUSE_READLINE"
	READLINE_LIBS="-lreadline"
fi
AC_SUBST(READLINE_CFLAGS)
AC_SUBST(READLINE_LIBS)

#
# add compiler and linker flags
#
AX_CFLAGS_GCC_OPTION(-fvisibility=hidden, VISIBILITY_CFLAGS)
AC_SUBST(VISIBILITY_CFLAGS)

CFLAGS="$CFLAGS -Wall"

AC_ARG_ENABLE([werror],
	[AS_HELP_STRING([--enable-werror],
		[Treat warnings as errors (default: disabled)])],
		[enable_werror="$enableval"],
		[enable_werror=no])
if test "x$enable_werror" = "xyes"; then
	CFLAGS="$CFLAGS -Werror"
fi

COMMANDS_ALIGN=32
AC_DEFINE_UNQUOTED([COMMANDS_ALIGN], [$COMMANDS_ALIGN],
	  [Define to the number of bytes to align commands to.])
AC_SUBST(COMMANDS_ALIGN)

AC_OUTPUT([
	bin/Makefile
	bin/remote-control-browser/Makefile
	bin/remote-control-client/Makefile
	bin/remote-control-client/remote-control-client.lds
	bin/remote-control-gtk/Makefile
	bin/remote-control/Makefile
	bin/utils/Makefile
	data/de.avionic-design.RemoteControl.service
	data/Makefile
	Makefile
	src/client/Makefile
	src/client/remote-control.pc
	src/common/Makefile
	src/Makefile
	src/server/Makefile
	tests/Makefile
])

echo "
remote-control was configured with the following options:

Features:
  readline support:          $use_readline
  Audio (UCM) Backend:       $with_audio_backend
  Backlight Backend:         $with_backlight_backend
  Video Backend:             $with_video_backend
  SmartCard/RFID Backend:    $with_smartcard_backend
  VoIP Backend:              $with_voip_backend
  Modem Backend:             $with_modem_backend
  Handset Backend:           $with_handset_backend
  GPIO Backend:              $with_gpio_backend

Options:
  Treat warnings as errors:  $enable_werror
  Gtk+ Version:              $with_gtk
  JavaScript Binding:        $enable_javascript"

AS_IF([test "x$enable_javascript" = "xyes"], [
	AS_ECHO("    IR Input Support:        $enable_javascript_ir")
])

AS_ECHO
