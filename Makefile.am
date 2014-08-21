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
	-I${top_srcdir}/libowfat \
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

libfreeq_1_0_la_SOURCES = \
	src/libfreeq.c \
	src/varint.c

noinst_LIBRARIES = libcontrol.a libowfat.a

libowfat_a_SOURCES = \
	libowfat/buffer/buffer_0.c \
	libowfat/buffer/buffer_feed.c \
	libowfat/buffer/errmsg_warn.c \
	libowfat/buffer/buffer_getline_sa.c \
	libowfat/buffer/errmsg_puts.c \
	libowfat/buffer/errmsg_warnsys.c \
	libowfat/buffer/buffer_putsaflush.c \
	libowfat/buffer/buffer_puterror.c \
	libowfat/buffer/buffer_putvarint.c \
	libowfat/buffer/buffer_2.c \
	libowfat/buffer/buffer_putspace.c \
	libowfat/buffer/errmsg_iam.c \
	libowfat/buffer/buffer_putsalign.c \
	libowfat/buffer/buffer_putlonglong.c \
	libowfat/buffer/errmsg_write.c \
	libowfat/buffer/errmsg_info.c \
	libowfat/buffer/buffer_init.c \
	libowfat/buffer/errmsg_infosys.c \
	libowfat/buffer/buffer_close.c \
	libowfat/buffer/buffer_mmapread.c \
	libowfat/buffer/buffer_puterror2.c \
	libowfat/buffer/buffer_putalign.c \
	libowfat/buffer/buffer_putflush.c \
	libowfat/buffer/buffer_get_token_sa.c \
	libowfat/buffer/buffer_putbyte.c \
	libowfat/buffer/buffer_putlong.c \
	libowfat/buffer/buffer_putnlflush.c \
	libowfat/buffer/buffer_putm_internal.c \
	libowfat/buffer/buffer_stubborn2.c \
	libowfat/buffer/buffer_putulong.c \
	libowfat/buffer/buffer_getvarint.c \
	libowfat/buffer/buffer_get_token_sa_pred.c \
	libowfat/buffer/buffer_fromsa.c \
	libowfat/buffer/buffer_puts.c \
	libowfat/buffer/buffer_peek.c \
	libowfat/buffer/buffer_put.c \
	libowfat/buffer/buffer_get_token_pred.c \
	libowfat/buffer/buffer_putsflush.c \
	libowfat/buffer/buffer_get_token.c \
	libowfat/buffer/buffer_0small.c \
	libowfat/buffer/buffer_getline.c \
	libowfat/buffer/buffer_seek.c \
	libowfat/buffer/buffer_1.c \
	libowfat/buffer/buffer_stubborn.c \
	libowfat/buffer/buffer_getc.c \
	libowfat/buffer/buffer_putm_internal_flush.c \
	libowfat/buffer/buffer_tosa.c \
	libowfat/buffer/buffer_getnewline_sa.c \
	libowfat/buffer/buffer_getn.c \
	libowfat/buffer/buffer_putulonglong.c \
	libowfat/buffer/buffer_flush.c \
	libowfat/buffer/buffer_get_new_token_sa_pred.c \
	libowfat/buffer/buffer_get.c \
	libowfat/buffer/buffer_putsa.c \
	libowfat/buffer/buffer_get_new_token_sa.c \
	libowfat/buffer/buffer_put8long.c \
	libowfat/buffer/buffer_putxlong.c \
	libowfat/buffer/buffer_init_free.c \
	libowfat/buffer/buffer_1small.c \
	libowfat/array/array_cate.c \
	libowfat/array/array_catb.c \
	libowfat/array/array_cats0.c \
	libowfat/array/array_cat0.c \
	libowfat/array/array_bytes.c \
	libowfat/array/array_length.c \
	libowfat/array/array_truncate.c \
	libowfat/array/array_cat.c \
	libowfat/array/array_trunc.c \
	libowfat/array/array_cats.c \
	libowfat/array/array_fail.c \
	libowfat/array/array_equal.c \
	libowfat/array/array_start.c \
	libowfat/array/array_reset.c \
	libowfat/array/array_allocate.c \
	libowfat/array/array_get.c \
	libowfat/io/io_readfile.c \
	libowfat/io/io_eagain.c \
	libowfat/io/io_waitread.c \
	libowfat/io/io_timeouted.c \
	libowfat/io/io_closeonexec.c \
	libowfat/io/iob_addbuf_munmap.c \
	libowfat/io/iob_free.c \
	libowfat/io/io_appendfile.c \
	libowfat/io/io_check.c \
	libowfat/io/iob_adds_free.c \
	libowfat/io/iob_addbuf.c \
	libowfat/io/io_canwrite.c \
	libowfat/io/io_socketpair.c \
	libowfat/io/io_waituntil2.c \
	libowfat/io/io_mmapwritefile.c \
	libowfat/io/io_wantread.c \
	libowfat/io/io_timeout.c \
	libowfat/io/io_wantwrite.c \
	libowfat/io/io_getcookie.c \
	libowfat/io/io_nonblock.c \
	libowfat/io/iob_prefetch.c \
	libowfat/io/iob_send.c \
	libowfat/io/io_dontwantread.c \
	libowfat/io/iob_new.c \
	libowfat/io/iob_addbuf_free.c \
	libowfat/io/io_trywritetimeout.c \
	libowfat/io/io_wait.c \
	libowfat/io/io_tryread.c \
	libowfat/io/io_sigpipe.c \
	libowfat/io/io_readwritefile.c \
	libowfat/io/io_passfd.c \
	libowfat/io/io_finishandshutdown.c \
	libowfat/io/iob_write.c \
	libowfat/io/iob_addfile_close.c \
	libowfat/io/io_waituntil.c \
	libowfat/io/io_block.c \
	libowfat/io/io_tryreadtimeout.c \
	libowfat/io/io_pipe.c \
	libowfat/io/io_trywrite.c \
	libowfat/io/io_sendfile.c \
	libowfat/io/iob_addfile.c \
	libowfat/io/io_receivefd.c \
	libowfat/io/io_fd.c \
	libowfat/io/io_close.c \
	libowfat/io/iob_adds.c \
	libowfat/io/io_dontwantwrite.c \
	libowfat/io/io_createfile.c \
	libowfat/io/io_waitwrite.c \
	libowfat/io/io_canread.c \
	libowfat/io/iob_bytesleft.c \
	libowfat/io/io_setcookie.c \
	libowfat/io/iob_reset.c \
	libowfat/io/iob_addbuf_internal.c \
	libowfat/uint/uint16_pack.c \
	libowfat/uint/uint64_read.c \
	libowfat/uint/uint64_pack.c \
	libowfat/uint/uint32_read.c \
	libowfat/uint/uint16_unpack.c \
	libowfat/uint/uint32_pack.c \
	libowfat/uint/uint32_read_big.c \
	libowfat/uint/uint64_unpack_big.c \
	libowfat/uint/uint32_pack_big.c \
	libowfat/uint/uint16_unpack_big.c \
	libowfat/uint/uint64_unpack.c \
	libowfat/uint/uint16_read_big.c \
	libowfat/uint/uint32_unpack.c \
	libowfat/uint/uint64_pack_big.c \
	libowfat/uint/uint32_unpack_big.c \
	libowfat/uint/uint16_pack_big.c \
	libowfat/uint/uint64_read_big.c \
	libowfat/uint/uint16_read.c \
	libowfat/tryinline.c \
	libowfat/trypoll.c \
	libowfat/textcode/fmt_uuencoded.c \
	libowfat/textcode/scan_to_array.c \
	libowfat/textcode/fmt_to_sa.c \
	libowfat/textcode/scan_base64.c \
	libowfat/textcode/scan_cescape.c \
	libowfat/textcode/scan_hexdump.c \
	libowfat/textcode/scan_tofrom_array.c \
	libowfat/textcode/fmt_ldapescape.c \
	libowfat/textcode/fmt_cescape.c \
	libowfat/textcode/scan_urlencoded.c \
	libowfat/textcode/scan_to_sa.c \
	libowfat/textcode/fmt_urlencoded.c \
	libowfat/textcode/fmt_yenc.c \
	libowfat/textcode/scan_quotedprintable.c \
	libowfat/textcode/fmt_ldapescape2.c \
	libowfat/textcode/fmt_hexdump.c \
	libowfat/textcode/scan_uuencoded.c \
	libowfat/textcode/scan_yenc.c \
	libowfat/textcode/fmt_foldwhitespace.c \
	libowfat/textcode/scan_ldapescape.c \
	libowfat/textcode/base64.c \
	libowfat/textcode/fmt_base64.c \
	libowfat/textcode/scan_html.c \
	libowfat/textcode/fmt_quotedprintable.c \
	libowfat/textcode/fmt_tofrom_array.c \
	libowfat/textcode/fmt_to_array.c \
	libowfat/textcode/fmt_html.c \
	libowfat/mmap/mmap_shared.c \
	libowfat/mmap/mmap_unmap.c \
	libowfat/mmap/mmap_private.c \
	libowfat/mmap/mmap_read.c \
	libowfat/tryepoll.c \
	libowfat/unix/ndelay_off.c \
	libowfat/unix/winsock2errno.c \
	libowfat/unix/iopause.c \
	libowfat/unix/ndelay_on.c \
	libowfat/cdb/cdb_make.c \
	libowfat/cdb/cdb_traverse.c \
	libowfat/cdb/cdb.c \
	libowfat/cdb/cdb_hash.c \
	libowfat/dns/dns_packet.c \
	libowfat/dns/dns_rcip.c \
	libowfat/dns/dns_dfd.c \
	libowfat/dns/dns_txt.c \
	libowfat/dns/dns_rcrw.c \
	libowfat/dns/dns_nd.c \
	libowfat/dns/dns_resolve.c \
	libowfat/dns/dns_transmit.c \
	libowfat/dns/dns_sortip.c \
	libowfat/dns/dns_nd6.c \
	libowfat/dns/dns_ip.c \
	libowfat/dns/dns_domain.c \
	libowfat/dns/dns_ip6.c \
	libowfat/dns/dns_ipq6.c \
	libowfat/dns/dns_mx.c \
	libowfat/dns/dns_ipq.c \
	libowfat/dns/dns_name.c \
	libowfat/dns/dns_sortip6.c \
	libowfat/dns/dns_random.c \
	libowfat/dns/dns_dtda.c \
	libowfat/mult/range_str2inbuf.c \
	libowfat/mult/range_arrayinbuf.c \
	libowfat/mult/umult16.c \
	libowfat/mult/imult64.c \
	libowfat/mult/imult16.c \
	libowfat/mult/umult32.c \
	libowfat/mult/range_str4inbuf.c \
	libowfat/mult/umult64.c \
	libowfat/mult/range_strinbuf.c \
	libowfat/mult/imult32.c \
	libowfat/tai/tai_now.c \
	libowfat/tai/tai_sub.c \
	libowfat/tai/tai_pack.c \
	libowfat/tai/tai_add.c \
	libowfat/tai/tai_uint.c \
	libowfat/tai/tai_unpack.c \
	libowfat/open/open_excl.c \
	libowfat/open/open_trunc.c \
	libowfat/open/open_append.c \
	libowfat/open/open_rw.c \
	libowfat/open/open_read.c \
	libowfat/open/open_write.c \
	libowfat/open/openreadclose.c \
	libowfat/open/readclose.c \
	libowfat/case/case_lowerb.c \
	libowfat/case/case_diffb.c \
	libowfat/case/case_starts.c \
	libowfat/case/case_diffs.c \
	libowfat/case/case_lowers.c \
	libowfat/byte/byte_rchr.c \
	libowfat/byte/byte_equal_notimingattack.c \
	libowfat/byte/byte_diff.c \
	libowfat/byte/byte_zero.c \
	libowfat/byte/byte_copyr.c \
	libowfat/byte/byte_copy.c \
	libowfat/byte/byte_chr.c \
	libowfat/fmt/fmt_minus.c \
	libowfat/fmt/fmt_long.c \
	libowfat/fmt/fmt_human.c \
	libowfat/fmt/fmt_fill.c \
	libowfat/fmt/fmt_ulonglong.c \
	libowfat/fmt/fmt_asn1derlength.c \
	libowfat/fmt/fmt_double.c \
	libowfat/fmt/fmt_utf8.c \
	libowfat/fmt/fmt_str.c \
	libowfat/fmt/fmt_longlong.c \
	libowfat/fmt/fmt_8longlong.c \
	libowfat/fmt/fmt_xlong.c \
	libowfat/fmt/fmt_strm_internal.c \
	libowfat/fmt/fmt_tohex.c \
	libowfat/fmt/fmt_ulong.c \
	libowfat/fmt/fmt_8long.c \
	libowfat/fmt/fmt_pad.c \
	libowfat/fmt/fmt_ulong0.c \
	libowfat/fmt/fmt_humank.c \
	libowfat/fmt/fmt_strn.c \
	libowfat/fmt/fmt_asn1dertag.c \
	libowfat/fmt/fmt_xlonglong.c \
	libowfat/fmt/fmt_plusminus.c \
	libowfat/fmt/fmt_httpdate.c \
	libowfat/str/str_diffn.c \
	libowfat/str/str_chr.c \
	libowfat/str/str_copy.c \
	libowfat/str/str_start.c \
	libowfat/str/str_rchr.c \
	libowfat/str/str_len.c \
	libowfat/str/str_diff.c \
	libowfat/socket/socket_local4.c \
	libowfat/socket/socket_udp4.c \
	libowfat/socket/fmt_ip6if.c \
	libowfat/socket/socket_ip4loopback.c \
	libowfat/socket/socket_bind6_reuse.c \
	libowfat/socket/socket_sctp4.c \
	libowfat/socket/socket_mcttl4.c \
	libowfat/socket/fmt_ip6.c \
	libowfat/socket/fmt_ip6ifc.c \
	libowfat/socket/socket_connect4.c \
	libowfat/socket/socket_deferaccept.c \
	libowfat/socket/socket_sctp4b.c \
	libowfat/socket/socket_accept4.c \
	libowfat/socket/scan_ip6.c \
	libowfat/socket/socket_bind4_reuse.c \
	libowfat/socket/socket_mcloop6.c \
	libowfat/socket/fmt_ip4.c \
	libowfat/socket/socket_mcjoin6.c \
	libowfat/socket/socket_send6.c \
	libowfat/socket/socket_v6any.c \
	libowfat/socket/init.c \
	libowfat/socket/socket_connected.c \
	libowfat/socket/socket_tcp4b.c \
	libowfat/socket/socket_remote4.c \
	libowfat/socket/socket_connect6.c \
	libowfat/socket/scan_ip6if.c \
	libowfat/socket/socket_mcloop4.c \
	libowfat/socket/socket_getifidx.c \
	libowfat/socket/socket_tcp6.c \
	libowfat/socket/socket_mchopcount6.c \
	libowfat/socket/socket_send4.c \
	libowfat/socket/socket_mcleave4.c \
	libowfat/socket/fmt_ip6_flat.c \
	libowfat/socket/socket_local6.c \
	libowfat/socket/socket_v6loopback.c \
	libowfat/socket/socket_sctp6b.c \
	libowfat/socket/scan_ip6_flat.c \
	libowfat/socket/scan_ip4.c \
	libowfat/socket/socket_noipv6.c \
	libowfat/socket/socket_remote6.c \
	libowfat/socket/socket_mcjoin4.c \
	libowfat/socket/socket_udp6.c \
	libowfat/socket/socket_recv6.c \
	libowfat/socket/socket_recv4.c \
	libowfat/socket/socket_broadcast.c \
	libowfat/socket/socket_tryreservein.c \
	libowfat/socket/socket_bind4.c \
	libowfat/socket/fmt_ip6c.c \
	libowfat/socket/socket_v4mappedprefix.c \
	libowfat/socket/socket_mcleave6.c \
	libowfat/socket/socket_sctp6.c \
	libowfat/socket/socket_getifname.c \
	libowfat/socket/socket_tcp4.c \
	libowfat/socket/socket_bind6.c \
	libowfat/socket/socket_listen.c \
	libowfat/socket/socket_accept6.c \
	libowfat/socket/socket_tcp6b.c \
	libowfat/stralloc/stralloc_copy.c \
	libowfat/stralloc/stralloc_zero.c \
	libowfat/stralloc/stralloc_free.c \
	libowfat/stralloc/stralloc_readyplus.c \
	libowfat/stralloc/stralloc_append.c \
	libowfat/stralloc/stralloc_cats.c \
	libowfat/stralloc/stralloc_catlong0.c \
	libowfat/stralloc/stralloc_init.c \
	libowfat/stralloc/stralloc_chomp.c \
	libowfat/stralloc/stralloc_cat.c \
	libowfat/stralloc/stralloc_catb.c \
	libowfat/stralloc/stralloc_diff.c \
	libowfat/stralloc/stralloc_copyb.c \
	libowfat/stralloc/stralloc_starts.c \
	libowfat/stralloc/stralloc_copys.c \
	libowfat/stralloc/stralloc_ready.c \
	libowfat/stralloc/stralloc_catulong0.c \
	libowfat/stralloc/stralloc_catm_internal.c \
	libowfat/stralloc/stralloc_chop.c \
	libowfat/stralloc/stralloc_diffs.c \
	libowfat/examples/buffer_getline.c \
	libowfat/examples/str.c \
	libowfat/examples/byte.c \
	libowfat/scan/scan_asn1dertag.c \
	libowfat/scan/scan_asn1derlength.c \
	libowfat/scan/scan_int.c \
	libowfat/scan/scan_ushort.c \
	libowfat/scan/scan_nonwhitenskip.c \
	libowfat/scan/scan_8short.c \
	libowfat/scan/scan_plusminus.c \
	libowfat/scan/scan_httpdate.c \
	libowfat/scan/scan_8long.c \
	libowfat/scan/scan_xint.c \
	libowfat/scan/scan_ulonglong.c \
	libowfat/scan/scan_whitenskip.c \
	libowfat/scan/scan_xlong.c \
	libowfat/scan/scan_noncharsetnskip.c \
	libowfat/scan/scan_longlong.c \
	libowfat/scan/scan_charsetnskip.c \
	libowfat/scan/scan_long.c \
	libowfat/scan/scan_double.c \
	libowfat/scan/scan_short.c \
	libowfat/scan/scan_8int.c \
	libowfat/scan/scan_xlonglong.c \
	libowfat/scan/scan_utf8.c \
	libowfat/scan/scan_xshort.c \
	libowfat/scan/scan_ulong.c \
	libowfat/scan/scan_uint.c \
	libowfat/scan/scan_fromhex.c \
	libowfat/taia/taia_add.c \
	libowfat/taia/taia_uint.c \
	libowfat/taia/taia_now.c \
	libowfat/taia/taia_addsec.c \
	libowfat/taia/taia_unpack.c \
	libowfat/taia/taia_approx.c \
	libowfat/taia/taia_pack.c \
	libowfat/taia/taia_sub.c \
	libowfat/taia/taia_frac.c \
	libowfat/taia/taia_tai.c \
	libowfat/taia/taia_less.c \
	libowfat/taia/taia_half.c

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

libfreeq_1_0_la_LIBADD = $(NANOMSG_LDFLAGS) $(GLIB_LIBS) libowfat.a

stest_SOURCES = src/stest.c src/system.h
freeqd_SOURCES = src/freeqd.cc src/system.h
freeql_SOURCES = src/freeql.cc src/system.h
system_monitor_SOURCES = src/system_monitor.c src/system.h

system_monitor_LDADD = \
	$(LIBINTL) \
	$(top_builddir)/lib/libfreeq.la \
	libfreeq-1.0.la \
	libowfat.a \
	$(NANOMSG_LDFLAGS) \
	$(GLIB_LIBS) \
	$(PROCPS_LIBS)

freeqd_LDADD = \
	$(LIBINTL) \
	libowfat.a \
	$(top_builddir)/lib/libfreeq.la \
	libcontrol.a \
	libfreeq-1.0.la \
	$(NANOMSG_LDFLAGS) \
	$(GLIB_LIBS) \
	$(SQLITE4_LDFLAGS)

freeql_LDADD = $(LIBINTL) $(top_builddir)/lib/libfreeq.la libfreeq-1.0.la  $(GLIB_LIBS)

stest_LDADD = $(LIBINTL) libcontrol.a $(top_builddir)/lib/libfreeq.la libfreeq-1.0.la $(GLIB_LIBS)

TESTS = check_basic check_msgpack

check_PROGRAMS = check_basic check_msgpack
check_basic_SOURCES = tests/check_basic.c src/varint.c $(top_builddir)/src/freeq/freeq.h
check_basic_CFLAGS = @CHECK_CFLAGS@
check_basic_LDADD = libfreeq-1.0.la @CHECK_LIBS@ @GLIB_LIBS@

check_msgpack_SOURCES = tests/check_msgpack.c $(top_builddir)/src/freeq/freeq.h
check_msgpack_CFLAGS = @CHECK_CFLAGS@
check_msgpack_LDADD = libfreeq-1.0.la @CHECK_LIBS@  @GLIB_LIBS@

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
