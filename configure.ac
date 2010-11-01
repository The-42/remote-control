AC_PREREQ(2.67)
AC_INIT([rc], [0.0.0], [bugs@avionic-design.de], [rc])
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

PKG_CHECK_MODULES(LIBRPC, librpc >= 0.2.1)
PKG_CHECK_MODULES(GLIB_GIO, glib-2.0 gio-2.0)

RPC_STUBGEN=rpc-stubgen
AC_SUBST(RPC_STUBGEN)

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
	Makefile
	src/Makefile
	src/client/Makefile
	src/server/Makefile
	bin/Makefile
	bin/remote-control/Makefile
])
