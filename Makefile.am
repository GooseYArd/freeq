# Process this file with automake to produce Makefile.in (in this,
# and all subdirectories).
# Makefile for the top-level directory of Freeq.
#
# Copyright 1997, 1998, 2005, 2006, 2007, 2008, 2009, 2010, 2011
# Free Software Foundation, Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.


# Find gnulib headers.
ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = \
	-include $(top_builddir)/config.h \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-DLIBEXECDIR=\""$(libexecdir)"\" \
	-I${top_srcdir}/gnulib \
	-I${top_srcdir}/gnulib/lib \
	-I${top_srcdir}/src/freeq \
	-I${top_srcdir}/src \
	-I/usr/include/glib-2.0 \
	-I/usr/lib/x86_64-linux-gnu/glib-2.0/include \
	$(SQLITE4_CFLAGS) \
	-DUNIX \
	-DENABLE_LOGGING \
	-DENABLE_DEBUG

AM_CFLAGS = ${my_CFLAGS} \
	-fvisibility=hidden \
	-ffunction-sections \
	-fdata-sections \
	-fPIC

AM_LDFLAGS = \
	-L/usr/lib/x86_64-linux-gnu \
	-Wl,--gc-sections \
	-Wl,--as-needed

DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

# Additional files to distribute.
EXTRA_DIST = \
	README-dev \
	ChangeLog.O \
	m4/gnulib-cache.m4

# Subdirectories to descend into.
SUBDIRS = po

bin_PROGRAMS = freeqd system_monitor freeql stest

lib_LTLIBRARIES = \
	libfreeq-1.0.la

libfreeq_1_0_la_SOURCES = src/libfreeq.c

noinst_LIBRARIES = libcontrol.a 

libcontrol_a_SOURCES = \
	control/qsutil.c \
	control/control.c \
	control/constmap.c \
	control/str_len.c \
	control/stralloc_opys.c \
	control/open_read.c \
	control/error.c \
	control/stralloc_copy.c \
	control/stralloc_pend.c \
	control/substdio.c \
	control/substdio_copy.c \
	control/getln.c \
	control/stralloc_catb.c \
	control/stralloc_opyb.c \
	control/stralloc_cats.c \
	control/stralloc_cat.c \
	control/byte_copy.c \
	control/byte_cr.c \
	control/byte_chr.c \
	control/stralloc_eady.c \
	control/alloc.c \
	control/alloc_re.c \
	control/getln2.c \
	control/getln.c \
	control/substdi.c \
	control/substdo.c


libfreeq_1_0_la_LIBADD = $(NANOMSG_LDFLAGS) $(MSGPACK_LIBS) $(PROCPS_LIBS) -lglib-2.0

stest_SOURCES = src/stest.c src/system.h
freeqd_SOURCES = src/freeqd.cc src/system.h
freeql_SOURCES = src/freeql.cc src/system.h
system_monitor_SOURCES = src/system_monitor.c src/system.h

system_monitor_LDADD = \
	$(LIBINTL) \
	$(top_builddir)/lib/libfreeq.la \
	libfreeq-1.0.la \
	$(NANOMSG_LDFLAGS) \
	$(MSGPACK_LIBS) \
	$(PROCPS_LIBS) \
	-lglib-2.0

freeqd_LDADD = \
	$(LIBINTL) \
	$(top_builddir)/lib/libfreeq.la \
	libcontrol.a \
	libfreeq-1.0.la \
	$(NANOMSG_LDFLAGS) \
	$(MSGPACK_LIBS) \
	$(SQLITE4_LDFLAGS)

freeql_LDADD = $(LIBINTL) $(top_builddir)/lib/libfreeq.la libfreeq-1.0.la 

stest_LDADD = $(LIBINTL) libcontrol.a $(top_builddir)/lib/libfreeq.la libfreeq-1.0.la -lglib-2.0

TESTS = check_basic check_msgpack

check_PROGRAMS = check_basic check_msgpack
check_basic_SOURCES = tests/check_basic.c $(top_builddir)/src/freeq/freeq.h
check_basic_CFLAGS = @CHECK_CFLAGS@
check_basic_LDADD = libfreeq-1.0.la @CHECK_LIBS@

check_msgpack_SOURCES = tests/check_msgpack.c $(top_builddir)/src/freeq/freeq.h
check_msgpack_CFLAGS = @CHECK_CFLAGS@
check_msgpack_LDADD = libfreeq-1.0.la @CHECK_LIBS@

LOG_COMPILER = $(SHELL)

AM_TESTS_ENVIRONMENT = \
  top_srcdir=$(top_srcdir) \
  PATH=.$(PATH_SEPARATOR)../src$(PATH_SEPARATOR)$$PATH \
  FREEQ=`echo freeq | sed '$(transform)'`; \
  export FREEQ PATH top_srcdir;

MOSTLYCLEANFILES =
noinst_LTLIBRARIES = 
BUILT_SOURCES = 
CLEANFILES =

include $(top_srcdir)/lib/local.mk

# Verify that all source files using _() are listed in po/POTFILES.in.
# The idea is to run this before making pretests, as well as official
# releases, so that translators will be sure to have all the messages.
# (From coreutils.)
po-check:
	if test -f po/POTFILES.in; then					\
	  grep -E -v '^(#|$$)' po/POTFILES.in				\
	    | grep -v '^src/false\.c$$' | sort > $@-1;			\
	  files=;							\
	  for file in $$($(CVS_LIST_EXCEPT)) `find * -name '*.[ch]'`; do \
	    case $$file in						\
	    djgpp/* | man/*) continue;;					\
	    esac;							\
	    case $$file in						\
	    *.[ch])							\
	      base=`expr " $$file" : ' \(.*\)\..'`;			\
	      { test -f $$base.l || test -f $$base.y; } && continue;;	\
	    esac;							\
	    files="$$files $$file";					\
	  done;								\
	  grep -E -l '\b(N?_|gettext *)\([^)"]*("|$$)' $$files		\
	    | sort -u > $@-2;						\
	  diff -u $@-1 $@-2 || exit 1;					\
	  rm -f $@-1 $@-2;						\
	fi


# Example of updating the online web pages for the documentation
# with the gendocs.sh script; see
# http://www.gnu.org/prep/maintain/html_node/Invoking-gendocs_002esh.html
#
gendocs = build-aux/gendocs.sh
gendocs_envvars = GENDOCS_TEMPLATE_DIR=doc
#
manual = freeq
manual_title = "Freeq Software Telemtry System"
email = $(PACKAGE_BUGREPORT)
gendocs_args = --email $(email) $(manual) $(manual_title)
#
www_target = $(HOME)/freeq/manual
#
doctemp = doc/wwwtemp
wwwdoc:
	rm -rf $(doctemp) && mkdir $(doctemp)
	cd $(doctemp) \
	&& ln -s ../*.texi . \
	&& env $(gendocs_envvars) $(gendocs) $(gendocs_args)
	cp -arf $(doctemp)/manual/. $(www_target)
	ls -ltu $(www_target)/html_node | tail  # $VCS rm -f obsolete files
# followed by $VCS add of new files and cvs commit.
