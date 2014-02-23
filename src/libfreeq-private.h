/*
  libfreeq - 

  Copyright (C) 2011 Someone <someone@example.com>

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef _LIBFREEQ_PRIVATE_H_
#define _LIBFREEQ_PRIVATE_H_

#include <stdbool.h>
#include <syslog.h>

#include <freeq/libfreeq.h>

static inline void __attribute__((always_inline, format(printf, 2, 3)))
freeq_log_null(struct freeq_ctx *ctx, const char *format, ...) {}

#define freeq_log_cond(ctx, prio, arg...) \
  do { \
    if (freeq_get_log_priority(ctx) >= prio) \
      freeq_log(ctx, prio, __FILE__, __LINE__, __FUNCTION__, ## arg); \
  } while (0)

#ifdef ENABLE_LOGGING
#  ifdef ENABLE_DEBUG
#    define dbg(ctx, arg...) freeq_log_cond(ctx, LOG_DEBUG, ## arg)
#  else
#    define dbg(ctx, arg...) freeq_log_null(ctx, ## arg)
#  endif
#  define info(ctx, arg...) freeq_log_cond(ctx, LOG_INFO, ## arg)
#  define err(ctx, arg...) freeq_log_cond(ctx, LOG_ERR, ## arg)
#else
#  define dbg(ctx, arg...) freeq_log_null(ctx, ## arg)
#  define info(ctx, arg...) freeq_log_null(ctx, ## arg)
#  define err(ctx, arg...) freeq_log_null(ctx, ## arg)
#endif

#ifndef HAVE_SECURE_GETENV
#  ifdef HAVE___SECURE_GETENV
#    define secure_getenv __secure_getenv
#  else
#    error neither secure_getenv nor __secure_getenv is available
#  endif
#endif

#define FREEQ_EXPORT __attribute__ ((visibility("default")))

void freeq_log(struct freeq_ctx *ctx,
           int priority, const char *file, int line, const char *fn,
           const char *format, ...)
           __attribute__((format(printf, 6, 7)));

#endif
