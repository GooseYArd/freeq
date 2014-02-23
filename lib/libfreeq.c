/*
  libfreeq - support library for Free Software Telemtry System

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
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

#include <freeq/libfreeq.h>
#include "libfreeq-private.h"
#include <msgpack.h>

/**
 * SECTION:libfreeq
 * @short_description: libfreeq context
 *
 * The context contains the default values for the library user,
 * and is passed to all library operations.
 */

/**
 * freeq_ctx:
 *
 * Opaque object representing the library context.
 */
struct freeq_ctx {
        int refcount;
        void (*log_fn)(struct freeq_ctx *ctx,
                       int priority, const char *file, int line, const char *fn,
                       const char *format, va_list args);
        void *userdata;
        int log_priority;
};

void freeq_log(struct freeq_ctx *ctx,
           int priority, const char *file, int line, const char *fn,
           const char *format, ...)
{
        va_list args;

        va_start(args, format);
        ctx->log_fn(ctx, priority, file, line, fn, format, args);
        va_end(args);
}

static void log_stderr(struct freeq_ctx *ctx,
                       int priority, const char *file, int line, const char *fn,
                       const char *format, va_list args)
{
        fprintf(stderr, "libfreeq: %s: ", fn);
        vfprintf(stderr, format, args);
}

/**
 * freeq_get_userdata:
 * @ctx: freeq library context
 *
 * Retrieve stored data pointer from library context. This might be useful
 * to access from callbacks like a custom logging function.
 *
 * Returns: stored userdata
 **/
FREEQ_EXPORT void *freeq_get_userdata(struct freeq_ctx *ctx)
{
        if (ctx == NULL)
                return NULL;
        return ctx->userdata;
}

/**
 * freeq_set_userdata:
 * @ctx: freeq library context
 * @userdata: data pointer
 *
 * Store custom @userdata in the library context.
 **/
FREEQ_EXPORT void freeq_set_userdata(struct freeq_ctx *ctx, void *userdata)
{
        if (ctx == NULL)
                return;
        ctx->userdata = userdata;
}

static int log_priority(const char *priority)
{
        char *endptr;
        int prio;

        prio = strtol(priority, &endptr, 10);
        if (endptr[0] == '\0' || isspace(endptr[0]))
                return prio;
        if (strncmp(priority, "err", 3) == 0)
                return LOG_ERR;
        if (strncmp(priority, "info", 4) == 0)
                return LOG_INFO;
        if (strncmp(priority, "debug", 5) == 0)
                return LOG_DEBUG;
        return 0;
}

/**
 * freeq_new:
 *
 * Create freeq library context. This reads the freeq configuration
 * and fills in the default values.
 *
 * The initial refcount is 1, and needs to be decremented to
 * release the resources of the freeq library context.
 *
 * Returns: a new freeq library context
 **/
FREEQ_EXPORT int freeq_new(struct freeq_ctx **ctx)
{
        const char *env;
        struct freeq_ctx *c;

        c = calloc(1, sizeof(struct freeq_ctx));
        if (!c)
                return -ENOMEM;

        c->refcount = 1;
        c->log_fn = log_stderr;
        c->log_priority = LOG_ERR;

        /* environment overwrites config */
        env = secure_getenv("FREEQ_LOG");
        if (env != NULL)
                freeq_set_log_priority(c, log_priority(env));

        info(c, "ctx %p created\n", c);
        dbg(c, "log_priority=%d\n", c->log_priority);
        *ctx = c;
        return 0;
}

/**
 * freeq_ref:
 * @ctx: freeq library context
 *
 * Take a reference of the freeq library context.
 *
 * Returns: the passed freeq library context
 **/
FREEQ_EXPORT struct freeq_ctx *freeq_ref(struct freeq_ctx *ctx)
{
        if (ctx == NULL)
                return NULL;
        ctx->refcount++;
        return ctx;
}

/**
 * freeq_unref:
 * @ctx: freeq library context
 *
 * Drop a reference of the freeq library context.
 *
 **/
FREEQ_EXPORT struct freeq_ctx *freeq_unref(struct freeq_ctx *ctx)
{
        if (ctx == NULL)
                return NULL;
        ctx->refcount--;
        if (ctx->refcount > 0)
                return NULL;
        info(ctx, "context %p released\n", ctx);
        free(ctx);
        return NULL;
}

/**
 * freeq_set_log_fn:
 * @ctx: freeq library context
 * @log_fn: function to be called for logging messages
 *
 * The built-in logging writes to stderr. It can be
 * overridden by a custom function, to plug log messages
 * into the user's logging functionality.
 *
 **/
FREEQ_EXPORT void freeq_set_log_fn(struct freeq_ctx *ctx,
                              void (*log_fn)(struct freeq_ctx *ctx,
                                             int priority, const char *file,
                                             int line, const char *fn,
                                             const char *format, va_list args))
{
        ctx->log_fn = log_fn;
        info(ctx, "custom logging function %p registered\n", log_fn);
}

/**
 * freeq_get_log_priority:
 * @ctx: freeq library context
 *
 * Returns: the current logging priority
 **/
FREEQ_EXPORT int freeq_get_log_priority(struct freeq_ctx *ctx)
{
        return ctx->log_priority;
}

/**
 * freeq_set_log_priority:
 * @ctx: freeq library context
 * @priority: the new logging priority
 *
 * Set the current logging priority. The value controls which messages
 * are logged.
 **/
FREEQ_EXPORT void freeq_set_log_priority(struct freeq_ctx *ctx, int priority)
{
  ctx->log_priority = priority;
}

struct freeq_column;
struct freeq_column *freeq_column_get_next(struct freeq_column *column);
const char *freeq_column_get_name(struct freeq_column *column);
const char *freeq_column_get_value(struct freeq_column *column);

struct freeq_table {
  struct freeq_ctx *ctx;
  const char *name;
  struct freeq_column *columns;
  int refcount;
};

FREEQ_EXPORT struct freeq_table *freeq_table_ref(struct freeq_table *table)
{
        if (!table)
                return NULL;
        table->refcount++;
        return table;
}

FREEQ_EXPORT struct freeq_table *freeq_table_unref(struct freeq_table *table)
{
        if (table == NULL)
                return NULL;
        table->refcount--;
        if (table->refcount > 0)
                return NULL;
        dbg(table->ctx, "context %p released\n", table);
        free(table);
        return NULL;
}

FREEQ_EXPORT struct freeq_ctx *freeq_table_get_ctx(struct freeq_table *table)
{
        return table->ctx;
}

FREEQ_EXPORT int freeq_table_new_from_string(struct freeq_ctx *ctx, const char *string, struct freeq_table **table)
{
        struct freeq_table *t;

        t = calloc(1, sizeof(struct freeq_table));
        if (!t)
                return -ENOMEM;

        t->refcount = 1;
        t->ctx = ctx;
        *table = t;
        return 0;
}

FREEQ_EXPORT int freeq_table_column_new(struct freeq_table *t, const char *string, struct freeq_column **column)
{
        struct freeq_column *c;

        c = calloc(1, sizeof(struct freeq_column));
        if (!c)
                return -ENOMEM;

        c->refcount = 1;
        c->table = t;
        *column = c;
        return 0;
}

FREEQ_EXPORT struct freeq_column *freeq_table_get_some_column(struct freeq_table *table)
{
        return NULL;
}

/* types I care about 
   ints
   strings
   ip addresses
   time_t values
   not sure about floats */

void pack(struct freeq_table *t) {

  /* creates buffer and serializer instance. */
  msgpack_sbuffer* buffer = msgpack_sbuffer_new();
  msgpack_packer* pk = msgpack_packer_new(buffer, msgpack_sbuffer_write);
  int j;

  for(j = 0; j<23; j++) {
    /* NB: the buffer needs to be cleared on each iteration */
    msgpack_sbuffer_clear(buffer);

    /* serializes ["Hello", "MessagePack"]. */
    msgpack_pack_array(pk, 3);

    msgpack_pack_raw(pk, 5);
    msgpack_pack_raw_body(pk, "Hello", 5);

    msgpack_pack_raw(pk, 11);
    msgpack_pack_raw_body(pk, "MessagePack", 11);

    msgpack_pack_int(pk, j);

    /* deserializes it. */
    msgpack_unpacked msg;
    msgpack_unpacked_init(&msg);
    bool success = msgpack_unpack_next(&msg, buffer->data, buffer->size, NULL);

    /* prints the deserialized object. */
    msgpack_object obj = msg.data;
    msgpack_object_print(stdout, obj);  /*=> ["Hello", "MessagePack"] */
    puts("");
  }

  /* cleaning */
  msgpack_sbuffer_free(buffer);
  msgpack_packer_free(pk);
}
