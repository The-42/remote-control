AC_PREREQ(2.66)
AC_INIT([remote-control], [1.0.1],
	[https://github.com/avionic-design/remote-control/issues],
	[remote-control], [https://github.com/avionic-design/remote-control])
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
AC_PROG_SED
AX_PROG_XSLTPROC
AM_NLS

PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.27.5 gio-2.0 gio-unix-2.0)
PKG_CHECK_MODULES(LIBNL, libnl-route-3.0)
PKG_CHECK_MODULES(POPPLER, poppler-glib)
PKG_CHECK_MODULES(GUDEV, gudev-1.0)
PKG_CHECK_MODULES(LIBNETTLE, nettle >= 3,
	[AC_DEFINE([HAVE_NETTLE3], [1], [Nettle >= 3 available])],
	[PKG_CHECK_MODULES(LIBNETTLE, nettle)]
)
PKG_CHECK_MODULES(LIBSOUP, libsoup-2.4)
PKG_CHECK_MODULES(GTKOSK, gtkosk,
		  [ac_gtkosk_datadir=`$PKG_CONFIG --variable=pkgdatadir gtkosk`])
AC_SUBST(GTKOSK_DATADIR, $ac_gtkosk_datadir)

AS_IF([test "x$USE_NLS" = "xyes"],
	[AC_DEFINE([ENABLE_NLS], [1], [Define to 1 if NLS support is enabled])],
	[AC_DEFINE([ENABLE_NLS], [0], [Define to 0 if NLS support is disabled])]
)

#
# Verbosity
#
if test x"$AM_DEFAULT_VERBOSITY" = x"0"; then
	if make --help 2>&1 | grep -q no-print-directory; then
		AM_MAKEFLAGS="$AM_MAKEFLAGS --no-print-directory"
	fi
fi
AC_SUBST(AM_MAKEFLAGS)

#
# Gtk+ version
#
AC_ARG_WITH([gtk],
	[AS_HELP_STRING([--with-gtk=@<:@2.0|3.0@:>@],
			[The Gtk+ version to use (default: 2.0)])
	], [], [with_gtk=2.0])

AS_CASE([$with_gtk],
	[2.0], [],
	[3.0], [
		AC_DEFINE([HAVE_GTKX_H], [1], [If Gtk+ 3.0 is used])
		AC_DEFINE([USE_GTK3], [1], [If Gtk+ 3.0 is used])
	],
	[AC_MSG_ERROR([Invalid Gtk+ version specified: must be 2.0 or 3.0])])

AC_ARG_WITH([webkit],
	[AS_HELP_STRING([--with-webkit=@<:@1.0|2.0@:>@],
			[The WebKit version to use (default: 1.0)])
	], [], [with_webkit=1.0])

AS_CASE([$with_webkit],
	[1.0], [],
	[2.0], [AC_DEFINE([USE_WEBKIT2], [1], [If WebKit2 API is used])],
	[AC_MSG_ERROR([Invalid Webkit2 version specified, must be 1.0 or 2.0])])

AS_CASE([$with_gtk:$with_webkit],
	[2.0:1.0], [REQUIRED_WEBKIT=webkit-1.0],
	[2.0:2.0], [AC_MSG_ERROR([WebKit2 requires Gtk+ 3.0])],
	[3.0:1.0], [REQUIRED_WEBKIT=webkitgtk-3.0],
	[3.0:2.0], [REQUIRED_WEBKIT=webkit2gtk-3.0])

PKG_CHECK_MODULES(GTK, gtk+-x11-$with_gtk x11 xext)
PKG_CHECK_MODULES(WEBKIT, $REQUIRED_WEBKIT)

#
# audio (UCM) backend
#
enable_audio_alsa=no

AC_ARG_WITH(audio_backend,
	[AS_HELP_STRING([--with-audio-backend=@<:@none/alsa/pulse@:>@],
		[Use the specified audio (UCM) backend. [default=none]])
	], [], [with_audio_backend=none])

AS_CASE([$with_audio_backend],
	[none], [],
	[alsa], [PKG_CHECK_MODULES(ALSA, alsa, [enable_audio_alsa=yes])],
	[pulse], [PKG_CHECK_MODULES(PULSE, [libpulse >= 3.0 libpulse-mainloop-glib >= 3.0], [enable_audio_pulse=yes])],
	[AC_MSG_ERROR([Invalid audio backend: must be none, alsa or pulse])]
)

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

#
# lldp monitor backend
#

AC_ARG_ENABLE([lldpctl],
	[AS_HELP_STRING([--enable-lldpctl],
			[Enable lldpctl support [default=no]])],
	[enable_lldpctl="$enableval"], [enable_lldpctl=no])
AS_IF([test "x$enable_lldpctl" = "xyes"],
	[PKG_CHECK_MODULES(LLDPCTL, lldpctl)])

#
# SmartCard backend
#

AC_ARG_ENABLE([libsmartcard],
	[AS_HELP_STRING([--enable-libsmartcard],
			[Enable libsmartcard support [default=no]])],
	[enable_libsmartcard="$enableval"], [enable_libsmartcard=no])
AS_IF([test "x$enable_libsmartcard" = "xyes"],
	[PKG_CHECK_MODULES(LIBSMARTCARD, libsmartcard)])
AS_IF([test "x$enable_libsmartcard" = "xyes"],
	[AC_DEFINE([ENABLE_LIBSMARTCARD], [1], [Define to 1 if smartcard should support libsmartcard])],
	[AC_DEFINE([ENABLE_LIBSMARTCARD], [0], [Define to 0 if smartcard should not support libsmartcard])]
)

AC_ARG_ENABLE([libpcsclite],
	[AS_HELP_STRING([--enable-libpcsclite],
			[Enable libpcsclite support [default=no]])],
	[enable_libpcsclite="$enableval"], [enable_libpcsclite=no])
AS_IF([test "x$enable_libpcsclite" = "xyes"],
	[PKG_CHECK_MODULES(LIBPCSCLITE, libpcsclite)])
AS_IF([test "x$enable_libpcsclite" = "xyes"],
	[AC_DEFINE([ENABLE_LIBPCSCLITE], [1], [Define to 1 if smartcard should support libpcsclite])],
	[AC_DEFINE([ENABLE_LIBPCSCLITE], [0], [Define to 0 if smartcard should not support libpcsclite])]
)

AC_ARG_ENABLE([smartcard_info],
	[AS_HELP_STRING([--enable-smartcard_info],
			[Enable reading smartcard info [default=no]])],
	[enable_smartcard_info="$enableval"], [enable_smartcard_info=no])
AS_IF([test "x$enable_smartcard_info" = "xyes"],
	[PKG_CHECK_MODULES(LIBXML2, libxml-2.0)])
AS_IF([test "x$enable_smartcard_info" = "xyes"],
	[PKG_CHECK_MODULES(ZLIB, zlib)])
AS_IF([test "x$enable_smartcard_info" = "xyes"],
	[AC_DEFINE([ENABLE_SMARTCARD_INFO], [1], [Define to 1 for smartcard info support])],
	[AC_DEFINE([ENABLE_SMARTCARD_INFO], [0], [Define to 0 for no smartcard info support])]
)

#
# Video backend
#
enable_libvlc=no
enable_gst=no
gst_version=unknown

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
	[gst], [
		PKG_CHECK_MODULES(GST, [gstreamer-1.0
					gstreamer-plugins-base-1.0
					gstreamer-video-1.0],
				       [gst_version=1.0 enable_gst=yes],[
			PKG_CHECK_MODULES(GST, [gstreamer-0.10
						gstreamer-plugins-base-0.10
						gstreamer-interfaces-0.10],
					       [gst_version=1.0 enable_gst=yes],
				[AC_MSG_ERROR([Unable to find gstreamer packages.])])
			])
	], [AC_MSG_ERROR([Invalid video backend: must be none, libvlc or gst])]
)

dnl FIXME: This is not the way to go, try to do this in AC_CASE[gst]
if test "x$enable_gst" = "xyes"; then
	PKG_CHECK_MODULES([XRANDR], [xrandr >= 1.3 xrender x11 xproto >= 7.0.17],
		[AC_DEFINE([ENABLE_XRANDR], [1], [Define to 1 when xrandr support is available.])])
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
	[linphone], [PKG_CHECK_MODULES(LINPHONE, linphone >= 3.7, [enable_linphone=yes])],
	[AC_MSG_ERROR([Invalid voip backend: must be none or linphone])]
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

AS_IF([test "x$enable_gpio_sysfs" = "xyes"], [PKG_CHECK_MODULES(LIBGPIO, libgpio >= 0.4.1)])

#
# JavaScript binding
#
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

# LCD TV Serial support
AC_MSG_CHECKING([whether to enable JavaScript LCD Serial support])
AC_ARG_ENABLE(javascript-lcd,
	[AS_HELP_STRING([--enable-javascript-lcd],
			[Enable JavaScript LCD Serial support (default: no)])],
	[], [enable_javascript_lcd=no])
AC_MSG_RESULT([$enable_javascript_lcd])

AS_IF([test "x$enable_javascript_lcd" = "xyes"], [
	AC_DEFINE([ENABLE_JAVASCRIPT_LCD], [1],
		  [whether JavaScript LCD Serial support is enabled])
])

# Watchdog support for the main webkit window.
AC_MSG_CHECKING([whether to enable JavaScript Application Watchdog support])
AC_ARG_ENABLE(javascript-app-watchdog,
	[AS_HELP_STRING([--enable-javascript-app-watchdog],
			[Enable JavaScript Application Watchdog support (default: no)])],
	[], [enable_javascript_app_watchdog=no])
AC_MSG_RESULT([$enable_javascript_app_watchdog])

AS_IF([test "x$enable_javascript_app_watchdog" = "xyes"], [
	AC_DEFINE([ENABLE_JAVASCRIPT_APP_WATCHDOG], [1],
		  [whether JavaScript Application Watchdog support is enabled])
])

#
# D-Bus Watchdog support
#
AC_MSG_CHECKING([whether to enable D-Bus watchdog support])
AC_ARG_ENABLE([watchdog],
	[AS_HELP_STRING([--enable-watchdog],
			[Enable D-Bus watchdog support [default=no]])],
	[], [enable_watchdog=no])
AC_MSG_RESULT([$enable_watchdog])

AS_IF([test "x$enable_watchdog" = "xyes"], [
	AC_DEFINE([ENABLE_WATCHDOG], [1], [whether D-Bus watchdog support is enabled])
	PKG_CHECK_MODULES(WATCHDOG, dbus-watchdog)
])

#
# define automake conditionals according to select backends
#
AM_CONDITIONAL(ENABLE_AUDIO_ALSA, [test "x$enable_audio_alsa" = "xyes"])
AM_CONDITIONAL(ENABLE_AUDIO_PULSE, [test "x$enable_audio_pulse" = "xyes"])
AM_CONDITIONAL(ENABLE_BACKLIGHT_DPMS, [test "x$enable_backlight_dpms" = "xyes"])
AM_CONDITIONAL(ENABLE_BACKLIGHT_MEDATOM, [test "x$enable_backlight_medatom" = "xyes"])
AM_CONDITIONAL(ENABLE_BACKLIGHT_SYSFS, [test "x$enable_backlight_sysfs" = "xyes"])
AM_CONDITIONAL(ENABLE_LIBVLC, [test "x$enable_libvlc" = "xyes"])
AM_CONDITIONAL(ENABLE_GST, [test "x$enable_gst" = "xyes"])
AM_CONDITIONAL(ENABLE_LLDPCTL, [test "x$enable_lldpctl" = "xyes"])
AM_CONDITIONAL(ENABLE_LIBPCSCLITE, [test "x$enable_libpcsclite" = "xyes"])
AM_CONDITIONAL(ENABLE_LIBSMARTCARD, [test "x$enable_libsmartcard" = "xyes"])
AM_CONDITIONAL(ENABLE_SMARTCARD_INFO, [test "x$enable_smartcard_info" = "xyes"])
AM_CONDITIONAL(ENABLE_LINPHONE, [test "x$enable_linphone" = "xyes"])
AM_CONDITIONAL(ENABLE_GPIO_GPIODEV, [test "x$enable_gpio_gpiodev" = "xyes"])
AM_CONDITIONAL(ENABLE_GPIO_SYSFS, [test "x$enable_gpio_sysfs" = "xyes"])
AM_CONDITIONAL(ENABLE_JAVASCRIPT_IR, [test "x$enable_javascript_ir" = "xyes"])
AM_CONDITIONAL(ENABLE_JAVASCRIPT_LCD, [test "x$enable_javascript_lcd" = "xyes"])
AM_CONDITIONAL(ENABLE_JAVASCRIPT_APP_WATCHDOG, [test "x$enable_javascript_app_watchdog" = "xyes"])
AM_CONDITIONAL(ENABLE_WATCHDOG, [test "x$enable_watchdog" = "xyes"])
AM_CONDITIONAL(ENABLE_WEBKIT2, [test "x$with_webkit" = "x2.0"])
AM_CONDITIONAL(ENABLE_GTK3, [test "x$with_gtk" = "x3.0"])

#
# check for required files and packages
#
AS_IF([test "x$enable_gpio_gpiodev" = "xyes"],
	[AC_CHECK_HEADERS([linux/gpiodev.h], [],
		[AC_MSG_ERROR([linux/gpiodev.h is required for gpiodev backend])]
	)])

AC_CHECK_HEADER([readline/readline.h])
AC_CHECK_LIB([readline], [readline], [use_readline=yes], [use_readline=no])
AS_IF([test "x$use_readline" = "xyes"], [
	READLINE_CFLAGS="-DUSE_READLINE"
	READLINE_LIBS="-lreadline"
])
AC_SUBST(READLINE_CFLAGS)
AC_SUBST(READLINE_LIBS)

#
# add compiler and linker flags
#
AX_CFLAGS_GCC_OPTION(-fvisibility=hidden, VISIBILITY_CFLAGS)
AC_SUBST(VISIBILITY_CFLAGS)

CFLAGS="$CFLAGS -Wall"

AC_ARG_ENABLE([werror],
	[AS_HELP_STRING([--disable-werror],
		[Do not treat warnings as errors (default: warnings are errors)])],
		[enable_werror="$enableval"],
		[enable_werror=yes])
AS_IF([test "x$enable_werror" = "xyes"], [CFLAGS="$CFLAGS -Werror"])

COMMANDS_ALIGN=32
AC_DEFINE_UNQUOTED([COMMANDS_ALIGN], [$COMMANDS_ALIGN],
	  [Define to the number of bytes to align commands to.])
AC_SUBST(COMMANDS_ALIGN)

AC_OUTPUT([
	bin/Makefile
	bin/remote-control-browser/Makefile
	bin/remote-control/Makefile
	bin/utils/Makefile
	data/de.avionic-design.RemoteControl.service
	data/Makefile
	doc/Makefile
	Makefile
	src/common/Makefile
	src/Makefile
	src/lib/Makefile
	tests/Makefile
])

AS_ECHO("
remote-control was configured with the following options:

Features:
  readline support:          $use_readline
  Audio (UCM) Backend:       $with_audio_backend
  Backlight Backend:         $with_backlight_backend")
AS_IF([test "x$enable_gst" = "xyes"],
	[AS_ECHO("  Video Backend:             $with_video_backend ($gst_version)")],
	[AS_ECHO("  Video Backend:             $with_video_backend")])

smartcard_none=yes
AS_ECHO_N("  SmartCard Backends:  ")
AS_IF([test "x$enable_libsmartcard" = "xyes"],
	[
	AS_ECHO_N(" libsmartcard")
	smartcard_none=no
	])
AS_IF([test "x$enable_libpcsclite" = "xyes"],
	[
	AS_ECHO_N(" libpcsclite")
	smartcard_none=no
	])
AS_IF([test "x$smartcard_none" = "xyes"],
	[AS_ECHO_N("      none")])
AS_ECHO("")
AS_ECHO("  Smartcard info function:   $enable_smartcard_info")
AS_ECHO("  VoIP Backend:              $with_voip_backend
  GPIO Backend:              $with_gpio_backend

Options:
  Treat warnings as errors:  $enable_werror
  Gtk+ Version:              $with_gtk
  WebKit Version:            $with_webkit
  JavaScript Binding:        (always)
    IR Input Support:        $enable_javascript_ir
    LCD Serial Support:      $enable_javascript_lcd
    Application Watchdog Support: $enable_javascript_app_watchdog
  D-Bus Watchdog support:    $enable_watchdog")
AS_IF([test "x$enable_lldpctl" = "xyes"],
	[AS_ECHO("  LLDP monitor:              lldpctl")])
AS_ECHO
