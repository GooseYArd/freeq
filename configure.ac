dnl Process this file with autoconf to produce a configure script.
dnl
dnl This file is free software; as a special exception the author gives
dnl unlimited permission to copy and/or distribute it, with or without
dnl modifications, as long as this notice is preserved.
dnl
dnl This program is distributed in the hope that it will be useful, but
dnl WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
dnl implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

AC_INIT([Freeq], [0.1], [gooseyard@gmail.com],,[http://go.c800colon5.com/freeq])

dnl Must come before AM_INIT_AUTOMAKE.
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([1.11.1 readme-alpha parallel-tests])
AC_PREREQ([2.62])

AC_CONFIG_MACRO_DIR([m4])

dnl Where to generate output; srcdir location.
AC_CONFIG_HEADERS([config.h:config.in])dnl Keep filename to 8.3 for MS-DOS.
AC_CONFIG_SRCDIR([src/freeq.c])

dnl AC_PROG_CC
AC_PROG_CC_C99
AC_PROG_CXX

dnl Since we use gnulib: gl_EARLY must be called as soon as possible after
dnl the C compiler is checked.  The others could be later, but we just
dnl keep everything together.
gl_EARLY
gl_INIT

LT_INIT

dnl Copyright will apply as long as these sources are in use, e.g., are
dnl being compiled, which is reasonable year to claim the copyright.
AC_DEFINE([COPYRIGHT_YEAR], [m4_esyscmd([date +%Y])], [year in copyright message])

AX_LIB_SQLITE4

AS_IF([test "x$ac_sqlite4_version" == "x"],
	    [AC_MSG_ERROR([Unable to locate sqlite4])])

AC_SEARCH_LIBS([nn_socket], [nanomsg], [], [
  AC_MSG_ERROR([unable to find the nn_socket() function])
])

AC_CHECK_HEADERS([nanomsg/nn.h nanomsg/survey.h],
			   [found_nanomsg_headers=yes; break;])

AS_IF([test "x$found_nanomsg_headers" != "xyes"],
	    [AC_MSG_ERROR([Unable to find the standard nanomsg headers])])

PKG_CHECK_MODULES(MSGPACK, msgpack)
PKG_CHECK_MODULES(PROCPS, libprocps)

AC_LANG_PUSH([C++])
PKG_CHECK_MODULES(GLOG, libglog)
AC_LANG_POP([C++])

AC_GNU_SOURCE
AC_CHECK_FUNCS([__secure_getenv secure_getenv])

dnl GNU help2man creates man pages from --help output; in many cases, this
dnl is sufficient, and obviates the need to maintain man pages separately.
dnl However, this means invoking executables, which we generally cannot do
dnl when cross-compiling, so we test to avoid that (the variable
dnl "cross_compiling" is set by AC_PROG_CC).
AS_IF([test $cross_compiling = no], [
  AM_MISSING_PROG([HELP2MAN], [help2man])
], [
  HELP2MAN=:
])

dnl i18n support from GNU gettext.
AM_GNU_GETTEXT_VERSION([0.18.1])
AM_GNU_GETTEXT([external])

AC_CONFIG_FILES([Makefile
           contrib/Makefile
           doc/Makefile
           lib/Makefile
           man/Makefile
           po/Makefile.in
           src/Makefile
           tests/Makefile])
AC_OUTPUT
