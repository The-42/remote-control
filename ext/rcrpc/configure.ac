AC_PREREQ(2.66)
AC_INIT([rcrpc], [0.1.0],
	[https://github.com/avionic-design/rcrpc/issues],
	[rcrpc], [https://github.com/avionic-design/rcrpc])
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
AM_NLS

PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.27.5)

PKG_CHECK_MODULES(LIBRPC, librpc >= 0.4.18)
RPC_STUBGEN=rpc-stubgen
AC_SUBST(RPC_STUBGEN)

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
# check for required files and packages
#
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
AX_APPEND_COMPILE_FLAGS([-fvisibility=hidden], VISIBILITY_CFLAGS)
AC_SUBST(VISIBILITY_CFLAGS)

AX_APPEND_COMPILE_FLAGS([-Wall -D_FORTIFY_SOURCE=2])
AX_APPEND_COMPILE_FLAGS([-fstack-protector-strong -Werror=format-security])

AC_ARG_ENABLE([werror],
	[AS_HELP_STRING([--disable-werror],
		[Do not treat warnings as errors (default: warnings are errors)])],
		[enable_werror="$enableval"],
		[enable_werror=yes])
AS_IF([test "x$enable_werror" = "xyes"], [AX_APPEND_COMPILE_FLAGS([-Werror])])

COMMANDS_ALIGN=32
AC_DEFINE_UNQUOTED([COMMANDS_ALIGN], [$COMMANDS_ALIGN],
	  [Define to the number of bytes to align commands to.])
AC_SUBST(COMMANDS_ALIGN)

AC_CONFIG_FILES([
	Makefile
	bin/Makefile
	bin/remote-control-client.lds
	src/Makefile
	src/rcrpc/Makefile
	src/rcrpc-client/Makefile
	src/rcrpc-client/rcrpc-client.pc
	tests/Makefile
])

AC_OUTPUT

AS_ECHO("
rcrpc was configured with the following options:

  Readline support:          $use_readline
  Treat warnings as errors:  $enable_werror")
AS_ECHO
