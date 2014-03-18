# Makefile.am for freeq/src.
#
# Copyright 1996, 1997, 2001, 2005, 2006, 2007, 2008 Free Software
# Foundation, Inc.
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

bin_PROGRAMS = freeqd system_monitor

lib_LTLIBRARIES = libfreeq-1.0.la
libfreeq_1_0_la_SOURCES = libfreeq.c

libfreeq_1_0_la_LIBADD = $(NANOMSG_LDFLAGS) $(MSGPACK_LIBS) $(PROCPS_LIBS)

freeqd_SOURCES = freeqd.cc system.h
system_monitor_SOURCES = system_monitor.cc system.h

system_monitor_LDADD = $(LIBINTL) $(top_builddir)/lib/libgnu.a libfreeq-1.0.la $(NANOMSG_LDFLAGS) $(MSGPACK_LIBS) $(PROCPS_LIBS)
freeqd_LDADD = $(LIBINTL) $(top_builddir)/lib/libgnu.a libfreeq-1.0.la $(NANOMSG_LDFLAGS) $(MSGPACK_LIBS) $(SQLITE4_LDFLAGS)

localedir = $(datadir)/locale

AM_CPPFLAGS = -I$(top_srcdir)/lib -I$(top_builddir)/lib $(SQLITE4_CFLAGS) -DENABLE_LOGGING -DENABLE_DEBUG
DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@
