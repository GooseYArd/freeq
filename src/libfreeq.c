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
#include <assert.h>

#include <freeq/libfreeq.h>
#include "libfreeq-private.h"
#include "msgpack.h"

#include <nanomsg/nn.h>
#include <nanomsg/pipeline.h>
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
	void* userdata;
	const char *identity;
	const char* url;
	const char* appname;
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
 * freeq_get_identity:
 * @ctx: freeq library context
 *
 * Retrieve stored data pointer from library context. This might be useful
 * to access from callbacks like a custom logging function.
 *
 * Returns: stored identity
 **/
FREEQ_EXPORT const char *freeq_get_identity(struct freeq_ctx *ctx)
{
	if (ctx == NULL)
		return NULL;
	return ctx->identity;
}

/**
 * freeq_set_identity:
 * @ctx: freeq library context
 * @identity: pointer to identity
 *
 * Set @identity in the library context.
 **/
FREEQ_EXPORT void freeq_set_identity(struct freeq_ctx *ctx, const char *identity)
{
	if (ctx == NULL)
		return;
	ctx->identity = identity;
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
FREEQ_EXPORT int freeq_new(struct freeq_ctx **ctx, const char *appname, const char *identity)
{
	const char *env;
	struct freeq_ctx *c;

	c = calloc(1, sizeof(struct freeq_ctx));
	if (!c)
		return -ENOMEM;

	c->refcount = 1;
	c->log_fn = log_stderr;
	c->log_priority = LOG_ERR;
	c->appname = appname;
	c->identity = identity;

	/* environment overwrites config */
	env = secure_getenv("FREEQ_LOG");
	if (env != NULL)
		freeq_set_log_priority(c, log_priority(env));

	if (identity == NULL)
		identity = secure_getenv("HOSTNAME");

	freeq_set_identity(c, identity ? identity : "unknown");

	info(c, "ctx %p created\n", c);
	//dbg(c, "log_priority=%d\n", c->log_priority);
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

struct freeq_column *freeq_column_get_next(struct freeq_column *column);
const char *freeq_column_get_name(struct freeq_column *column);
const char *freeq_column_get_value(struct freeq_column *column);

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

	if (table->next != NULL)
		freeq_table_unref(table->next);

	table->refcount--;
	if (table->refcount > 0)
		return NULL;

	struct freeq_column *c = table->columns;
	struct freeq_column *next;
	while (c != NULL)
	{
		next = c->next;
		freeq_column_unref(c);
		c = next;
	}

	//dbg(table->ctx, "context %p released\n", table);
	free(table);
	return NULL;
}

FREEQ_EXPORT struct freeq_column *freeq_column_unref(struct freeq_column *column)
{
	if (column == NULL)
		return NULL;
	column->refcount--;
	if (column->refcount > 0)
		return NULL;

	struct freeq_column_segment *s = column->segments;
	struct freeq_column_segment *next;
	while (s != NULL)
	{
		next = s->next;
		freeq_column_segment_unref(s);
		s = next;
	}

	// dbg(table->ctx, "context %p released\n", table);
	free(column);
	return NULL;
}

FREEQ_EXPORT struct freeq_column_segment *freeq_column_segment_unref(struct freeq_column_segment *segment)
{
	if (segment == NULL)
		return NULL;
	segment->refcount--;
	if (segment->refcount > 0)
		return NULL;

	// dbg(table->ctx, "context %p released\n", table);
	free(segment);
	return NULL;
}

FREEQ_EXPORT struct freeq_ctx *freeq_table_get_ctx(struct freeq_table *table)
{
	return table->ctx;
}

FREEQ_EXPORT int freeq_table_new_from_string(struct freeq_ctx *ctx, const char *name, struct freeq_table **table)
{
	struct freeq_table *t;

	t = calloc(1, sizeof(struct freeq_table));
	if (!t)
		return -ENOMEM;

	t->name = name;
	t->numcols = 0;
	t->numrows = 0;
	t->refcount = 1;
	t->ctx = ctx;
	*table = t;
	return 0;
}

FREEQ_EXPORT int freeq_table_column_new(struct freeq_table *table, const char *name, freeq_coltype_t coltype, void *data)
{
	struct freeq_column *c;
	struct freeq_column_segment *seg;
	c = calloc(1, sizeof(struct freeq_column));
	if (!c)
		return -ENOMEM;

	seg = calloc(1, sizeof(struct freeq_column_segment));
	if (!seg) {
		free(c);
		return -ENOMEM;
	}

	c->name = name;
	c->refcount = 1;
	seg->len = table->numrows;
	seg->data = data;
	seg->refcount = 1;
	seg->next = NULL;
	c->segments = seg;
	c->coltype = coltype;
	table->numcols++;

	struct freeq_column *lastcol = table->columns;

	if (lastcol == NULL)
		table->columns = c;
	else
	{
		while (lastcol->next != NULL)
			lastcol = lastcol->next;
		lastcol->next = c;
	}
	return 0;
}

FREEQ_EXPORT struct freeq_column *freeq_table_get_some_column(struct freeq_table *table)
{
	return NULL;
}

FREEQ_EXPORT int freeq_table_send(struct freeq_ctx *ctx, struct freeq_table *table)
{
	msgpack_sbuffer sbuf;
	int res;
	msgpack_sbuffer_init(&sbuf);
	table->identity = ctx->identity;
	res = freeq_table_pack_msgpack(&sbuf, ctx, table);

	printf("freeq_table_send: table pack returned %d\n", res);

	if (res == 0) {
		const char *url = "ipc:///tmp/freeqd.ipc";
		int sock = nn_socket(AF_SP, NN_PUSH);
		assert(sock >= 0);
		assert(nn_connect(sock, url) >= 0);
		printf("sending \"%d\" btyes to %s\n", sbuf.size, url);
		int bytes = nn_send(sock, sbuf.data, sbuf.size, 0);
		printf("sent \"%d\" btyes\n", bytes);
		assert(bytes == sbuf.size);
		nn_shutdown(sock, 0);
	}

	msgpack_sbuffer_destroy(&sbuf);
}

FREEQ_EXPORT int freeq_table_pack_msgpack(msgpack_sbuffer *sbuf, struct freeq_ctx *ctx, struct freeq_table *table)
{
	struct freeq_column *col = table->columns;
	struct freeq_column_segment *seg;
	msgpack_packer pk;
	msgpack_packer_init(&pk, sbuf, msgpack_sbuffer_write);

	int len;

	dbg(ctx, "freeq_table_pack_msgpack: identity %s table %s %d cols %d rows\n", \
	    ctx->identity, table->name, table->numcols, table->numrows);

	msgpack_pack_int(&pk, table->numrows);

	if (ctx->identity == NULL)
	{
		dbg(ctx, "freeq_table_pack_msgpack: can't pack while ctx->identity is unset\n");
		return 1;
	}

	msgpack_pack_raw(&pk, strlen(ctx->identity));
	msgpack_pack_raw_body(&pk, ctx->identity, strlen(ctx->identity));
	msgpack_pack_raw(&pk, strlen(table->name));
	msgpack_pack_raw_body(&pk, table->name, strlen(table->name));

	msgpack_pack_array(&pk, table->numcols);
	while (col != NULL) {
		//dbg(ctx, "col %s type %d\n", col->name, col->coltype);
		msgpack_pack_int(&pk, col->coltype);
		col = col->next;
	}

	col = table->columns;
	msgpack_pack_array(&pk, table->numcols);
	while (col != NULL) {
		len = strlen(col->name);
		msgpack_pack_raw(&pk, len);
		msgpack_pack_raw_body(&pk, col->name, len);
		//dbg(ctx, "col name %s len %d\n", col->name, len);
		col = col->next;
	}

	col = table->columns;
	while (col != NULL) {
		//dbg(ctx, "packing column %s\n", i->name);
		msgpack_pack_array(&pk, table->numrows);
		seg = col->segments;
		//for (int j = 0; j < table->numrows; j++) {
		while (seg != NULL) {
			for (int i = 0; i < seg->len; i++) {
				switch (col->coltype)
				{
				case FREEQ_COL_STRING:
					len = strlen(((const char**)seg->data)[i]);
					msgpack_pack_raw(&pk, len);
					msgpack_pack_raw_body(&pk, ((const char **)seg->data)[i], len);
					break;
				case FREEQ_COL_NUMBER:
					msgpack_pack_int(&pk, ((int *)seg->data)[i]);
					break;
				case FREEQ_COL_IPV4ADDR:
					break;
				case FREEQ_COL_TIME:
					break;
				default:
					break;
				}
			}
			seg = seg->next;
		}
		col = col->next;
	}

	dbg(ctx, "packed buffer size is: %d\n", sbuf->size);
	return 0;

}

int freeq_unpack_string(struct freeq_ctx* ctx, char *buf, size_t bufsize, size_t *offset, char **string)
{
	int bsize = -1;
	char *s;
	int res = 0;
	msgpack_unpacked obj;
	msgpack_unpacked_init(&obj);

	//dbg(ctx, "trying to unpack a string\n");
	if (msgpack_unpack_next(&obj, buf, bufsize, offset)) {
		//dbg(ctx, "unpack_next succeeded\n");
		if (obj.data.type == MSGPACK_OBJECT_RAW) {
			//dbg(ctx, "type is good\n");
			bsize = obj.data.via.raw.size;
			s = calloc(1, bsize);
			if (!s)
				return -ENOMEM;
			memcpy((void *)s, obj.data.via.raw.ptr, bsize);
			//dbg(ctx, "all is well, returning %s\n", s);
			*string = s;
		} else {
			//dbg(ctx, "type was incorrect for object\n");
			res = 1;
		}
	}
	msgpack_unpacked_destroy(&obj);
	return res;
}

int freeq_attach_all_segments(struct freeq_column *from, struct freeq_column *to) {
	int count = 0;
	struct freeq_column_segment *tail = to->segments;

	while (tail->next != NULL)
		tail = tail->next;

	tail->next = from->segments;
	tail = from->segments;
	while (tail != NULL) {
		count++;
		tail->refcount++;
	}
	return count;
}

int freeq_unpack_int(struct freeq_ctx* ctx, char *buf, size_t bufsize, size_t *offset, int *val)
{
	int err = 0;
	msgpack_unpacked obj;
	msgpack_unpacked_init(&obj);

	//dbg(ctx, "trying to unpack an array of ints\n");
	if (msgpack_unpack_next(&obj, buf, bufsize, offset)) {
		if (obj.data.type < MSGPACK_OBJECT_NEGATIVE_INTEGER) {
			*val = obj.data.via.u64;
		} else {
			err = 1;
		}
	}
	msgpack_unpacked_destroy(&obj);
	return err;
}

int freeq_unpack_int_array(struct freeq_ctx* ctx, char *buf, size_t bufsize, size_t *offset, struct freeq_column *colp)
{
	int *vals;
	int err;
	msgpack_unpacked obj;
	msgpack_unpacked_init(&obj);

	//dbg(ctx, "trying to unpack an array of ints\n");
	if (msgpack_unpack_next(&obj, buf, bufsize, offset)) {
		if (obj.data.type == MSGPACK_OBJECT_ARRAY) {
			//dbg(ctx, "type is good\n");
			colp->segments->len = obj.data.via.array.size;
			vals = (int *)calloc(sizeof(int), colp->segments->len);
			if (!vals) {
				//dbg(ctx, "failed to allocate array of ints\n", elems);
				err = -ENOMEM;
			} else {
				//dbg(ctx, "allocated an array of size %d\n", elems);
				for (int i=0; i < obj.data.via.array.size; i++) {
					vals[i] = obj.data.via.array.ptr[i].via.u64;
				}
			}
		} else {
			err = 1;
			//dbg(ctx, "object is not an array\n");
		}
	}

	//arr = (int **)vals;
	colp->segments->data = (void *)vals;
	//dbg(ctx, "success\n");
	msgpack_unpacked_destroy(&obj);
	return err;
}

int freeq_unpack_string_array(struct freeq_ctx* ctx, char *buf, size_t bufsize, size_t *offset, struct freeq_column *colp)
{
	int bsize = -1;
	char *s;
	char **strings;
	int err = 0;
	msgpack_unpacked obj;
	msgpack_unpacked_init(&obj);

	//dbg(ctx, "trying to unpack an array of strings\n");
	if (msgpack_unpack_next(&obj, buf, bufsize, offset)) {
		if (obj.data.type == MSGPACK_OBJECT_ARRAY) {
			//dbg(ctx, "type is good\n");
			colp->segments->len = obj.data.via.array.size;
			strings = (char **)calloc(sizeof(char*), colp->segments->len);
			if (!strings) {
				err = -ENOMEM;
			} else {
				//dbg(ctx, "allocated an array of size %d\n", colp->segments->len);
				for (int i=0; i < obj.data.via.array.size; i++) {
					bsize = obj.data.via.array.ptr[i].via.raw.size;
					s = (char *)calloc(bsize+1, 1);
					memcpy((void *)s, obj.data.via.array.ptr[i].via.raw.ptr, bsize);
					strings[i] = s;
					//printf("STRING IS: %s, bsize was %d\n", strings[i], bsize);
				}


			}
		} else {
			err = 1;
			dbg(ctx, "object is not an array\n");
		}
	}

	colp->segments->data = (void *)strings;
	msgpack_unpacked_destroy(&obj);
	return err;
}

FREEQ_EXPORT int freeq_table_header_from_msgpack(struct freeq_ctx *ctx, char *buf, size_t bufsize, struct freeq_table **table)
{
	int bsize = -1;
	size_t offset = 0;

	int res;
	int err;
	int numrows;

	char *name, *identity;
	int numcols;
	char *s;

	struct freeq_table *tblp;
	struct freeq_column *colp;

	msgpack_unpacked obj;
	msgpack_unpacked_init(&obj);

	res = freeq_unpack_int(ctx, buf, bufsize, &offset, &numrows);
	if (res)
		return res;

	res = freeq_unpack_string(ctx, buf, bufsize, &offset, &identity);
	if (res)
		return res;

	res = freeq_unpack_string(ctx, buf, bufsize, &offset, &name);
	if (res)
		return res;

	freeq_table_new_from_string(ctx, name, &tblp);
	if (!tblp) {
		free(name);
		free(identity);
		return -ENOMEM;
	}

	/*
	  create columns:
	  the first array in the message contains integers representing the column type
	*/
	if (msgpack_unpack_next(&obj, buf, bufsize, &offset)) {
		if (obj.data.type == MSGPACK_OBJECT_ARRAY) {
			numcols = obj.data.via.array.size;
			for (int i=0; i < numcols; i++) {
				err = freeq_table_column_new(tblp, NULL, obj.data.via.array.ptr[i].via.u64, NULL);
				if (err < 0)
					exit(EXIT_FAILURE);
			}
		} else {
			err = 1;
			//dbg(ctx, "object is not an array\n");
		}
	}

	/*
	  populate column names
	*/
	if (msgpack_unpack_next(&obj, buf, bufsize, &offset)) {
		if (obj.data.type == MSGPACK_OBJECT_ARRAY) {
			if (numcols == obj.data.via.array.size) {
				colp = tblp->columns;
				for (int i=0; i < numcols; i++) {
					bsize = obj.data.via.array.ptr[i].via.raw.size;
					s = (char *)calloc(bsize, 1);
					memcpy((void *)s, obj.data.via.array.ptr[i].via.raw.ptr, bsize);
					colp->name = s;
					colp = colp->next;
				}
			}
		} else {
			err = 1;
			//dbg(ctx, "object is not an array\n");
		}
	}

	/*
	  unpack column data
	*/

	dbg(ctx, "unpacking column data...\n");
	colp = tblp->columns;
	while (colp != NULL) {
		switch (colp->coltype)
		{
		case FREEQ_COL_STRING:
			dbg(ctx, "unpacking string column %s\n", colp->name);
			res = freeq_unpack_string_array(ctx, buf, bufsize, &offset, colp);
			break;
		case FREEQ_COL_NUMBER:
			dbg(ctx, "unpacking INTEGER column %s\n", colp->name);
			res = freeq_unpack_int_array(ctx, buf, bufsize, &offset, colp);
			break;
		case FREEQ_COL_IPV4ADDR:
			break;
		case FREEQ_COL_TIME:
			break;
		default:
			break;
		}
		printf("address of column %s's segment is: %p\n", colp->name, colp->segments->data);
		colp = colp->next;
	}

	colp = tblp->columns;
	while (colp != NULL) {
		dbg(ctx, "AFTER THE MESS @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ %s\n", colp->name);
		colp = colp->next;
	}

	/* if (res) { */
	/*	//dbg(ctx, "failed to unpack column names: %d\n", res); */
	/*	freeq_table_header_unref(ctx, th); */
	/*	return res; */
	/* } */

	//dbg(ctx, "success\n");
	tblp->numrows = numrows;
	tblp->identity = identity;
	tblp->name = name;
	*table = tblp;

	return 0;
}
