# ===========================================================================
#      http://www.gnu.org/software/autoconf-archive/ax_lib_sqlite4.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_LIB_SQLITE4([MINIMUM-VERSION])
#
# DESCRIPTION
#
#   Test for the SQLite 4 library of a particular version (or newer)
#
#   This macro takes only one optional argument, required version of SQLite
#   4 library. If required version is not passed, 4.0.0 is used in the test
#   of existance of SQLite 4.
#
#   If no intallation prefix to the installed SQLite library is given the
#   macro searches under /usr, /usr/local, and /opt.
#
#   This macro calls:
#
#     AC_SUBST(SQLITE4_CFLAGS)
#     AC_SUBST(SQLITE4_LDFLAGS)
#     AC_SUBST(SQLITE4_VERSION)
#
#   And sets:
#
#     HAVE_SQLITE4
#
# LICENSE
#
#   Copyright (c) 2008 Mateusz Loskot <mateusz@loskot.net>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.

#serial 14

AC_DEFUN([AX_LIB_SQLITE4],
[
    AC_ARG_WITH([sqlite4],
        AS_HELP_STRING(
            [--with-sqlite4=@<:@ARG@:>@],
            [use SQLite 4 library @<:@default=yes@:>@, optionally specify the prefix for sqlite4 library]
        ),
        [
        if test "$withval" = "no"; then
            WANT_SQLITE4="no"
        elif test "$withval" = "yes"; then
            WANT_SQLITE4="yes"
            ac_sqlite4_path=""
        else
            WANT_SQLITE4="yes"
            ac_sqlite4_path="$withval"
        fi
        ],
        [WANT_SQLITE4="yes"]
    )

    SQLITE4_CFLAGS=""
    SQLITE4_LDFLAGS=""
    SQLITE4_VERSION=""

    if test "x$WANT_SQLITE4" = "xyes"; then

        ac_sqlite4_header="sqlite4.h"

        sqlite4_version_req=ifelse([$1], [], [4.0.0], [$1])
        sqlite4_version_req_shorten=`expr $sqlite4_version_req : '\([[0-9]]*\.[[0-9]]*\)'`
        sqlite4_version_req_major=`expr $sqlite4_version_req : '\([[0-9]]*\)'`
        sqlite4_version_req_minor=`expr $sqlite4_version_req : '[[0-9]]*\.\([[0-9]]*\)'`
        sqlite4_version_req_micro=`expr $sqlite4_version_req : '[[0-9]]*\.[[0-9]]*\.\([[0-9]]*\)'`
        if test "x$sqlite4_version_req_micro" = "x" ; then
            sqlite4_version_req_micro="0"
        fi

        sqlite4_version_req_number=`expr $sqlite4_version_req_major \* 1000000 \
                                   \+ $sqlite4_version_req_minor \* 1000 \
                                   \+ $sqlite4_version_req_micro`

        AC_MSG_CHECKING([for Sqlite4 library >= $sqlite4_version_req])

        if test "$ac_sqlite4_path" != ""; then
            ac_sqlite4_ldflags="-L$ac_sqlite4_path/lib"
            ac_sqlite4_cppflags="-I$ac_sqlite4_path/include"
        else
            for ac_sqlite4_path_tmp in /usr /usr/local /opt ; do
                if test -f "$ac_sqlite4_path_tmp/include/$ac_sqlite4_header" \
                    && test -r "$ac_sqlite4_path_tmp/include/$ac_sqlite4_header"; then
                    ac_sqlite4_path=$ac_sqlite4_path_tmp
                    ac_sqlite4_cppflags="-I$ac_sqlite4_path_tmp/include"
                    ac_sqlite4_ldflags="-L$ac_sqlite4_path_tmp/lib"
                    break;
                fi
            done
        fi

        ac_sqlite4_ldflags="$ac_sqlite4_ldflags -lsqlite4"

        saved_CPPFLAGS="$CPPFLAGS"
        CPPFLAGS="$CPPFLAGS $ac_sqlite4_cppflags"

        AC_LANG_PUSH(C)
        AC_COMPILE_IFELSE(
            [
            AC_LANG_PROGRAM([[@%:@include <sqlite4.h>]],
                [[
#if (SQLITE_VERSION_NUMBER >= $sqlite4_version_req_number)
/* Everything is okay */
#else
#  error SQLite version is too old
#endif
                ]]
            )
            ],
            [
            AC_MSG_RESULT([yes])
            success="yes"
            ],
            [
            AC_MSG_RESULT([not found])
            success="no"
            ]
        )
        AC_LANG_POP(C)

        CPPFLAGS="$saved_CPPFLAGS"

        if test "$success" = "yes"; then

            SQLITE4_CFLAGS="$ac_sqlite4_cppflags"
            SQLITE4_LDFLAGS="$ac_sqlite4_ldflags"

            ac_sqlite4_header_path="$ac_sqlite4_path/include/$ac_sqlite4_header"

            dnl Retrieve SQLite release version
            if test "x$ac_sqlite4_header_path" != "x"; then
                ac_sqlite4_version=`cat $ac_sqlite4_header_path \
                    | grep '#define.*SQLITE_VERSION.*\"' | sed -e 's/.* "//' \
                        | sed -e 's/"//'`
                if test $ac_sqlite4_version != ""; then
                    SQLITE4_VERSION=$ac_sqlite4_version
                else
                    AC_MSG_WARN([Cannot find SQLITE_VERSION macro in sqlite4.h header to retrieve SQLite version!])
                fi
            fi

            AC_SUBST(SQLITE4_CFLAGS)
            AC_SUBST(SQLITE4_LDFLAGS)
            AC_SUBST(SQLITE4_VERSION)
            AC_DEFINE([HAVE_SQLITE4], [], [Have the SQLITE4 library])
        fi
    fi
])
