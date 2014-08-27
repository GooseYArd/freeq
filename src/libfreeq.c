/*
  libfreeq - support library for Free Software Telemtry System

  Copyright (C) 2014 Andy Bailey <gooseyard@gmail.com>

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
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <stdbool.h>
#include <math.h>

#include <freeq/libfreeq.h>
#include "libfreeq-private.h"

#include <nanomsg/nn.h>
#include <nanomsg/pipeline.h>

#include "varint.h"
#include "libowfat/buffer.h"

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

FREEQ_EXPORT void freeq_log(struct freeq_ctx *ctx,
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

//struct freeq_column *freeq_column_get_next(struct freeq_column *column);
//const char *freeq_column_get_name(struct freeq_column *column);
//const char *freeq_column_get_value(struct freeq_column *column);

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

	free(table->name);
	for (int i=0; i < table->numcols; i++)
		free(table->columns[i].name);
	
	if (table->destroy_data)
	{
		for (int i=0; i < table->numcols; i++) 
		{
			dbg(table->ctx, "freeing column %d data\n", i);
			if (table->columns[i].coltype == FREEQ_COL_STRING)
				g_slist_free_full(table->columns[i].data, g_free);
			else
				g_slist_free(table->columns[i].data);
		}
	} 
	else 
		dbg(table->ctx, "destroy_data not set, not freeing column data\n");

	dbg(table->ctx, "table %p released\n", table);
	free(table);
	return NULL;
}


FREEQ_EXPORT struct freeq_ctx *freeq_table_get_ctx(struct freeq_table *table)
{
	return table->ctx;
}

FREEQ_EXPORT int freeq_error_write_sock(struct freeq_ctx *ctx, const char *errmsg, int sock)
{
	struct freeq_table errtbl;

	//freeq_coltype_t coltypes[] = { FREEQ_COL_STRING };
	//char *colnames[] = { "error" };

	errtbl.name = "error";
	errtbl.numcols = 1;
	errtbl.numrows = 1;

	dbg(ctx, "generated error table, sending...\n");
	return freeq_table_write(ctx, &errtbl, sock);
}

bool ragged(int c, int rlens[], int *min) {
	*min = rlens[0];
	bool ragged = false;

	for (int i = 0; i < c; i++) {
		if (rlens[i] < *min) {
			*min = rlens[i];
			ragged = true;
		}
	}
	return ragged;
}

FREEQ_EXPORT int freeq_table_new(struct freeq_ctx *ctx,
				 const char *name,
				 int numcols,
				 freeq_coltype_t coltypes[],
				 const char *colnames[],				 
				 struct freeq_table **table,
				 bool destroy_data,
				 ...)
{
	va_list argp;
	int collens[numcols];
	struct freeq_table *t;
	struct freeq_column *cols;
	
	t = (struct freeq_table *)malloc(sizeof(struct freeq_table) + (numcols * sizeof(struct freeq_column)));
	
	if (!t) {
		err(ctx, "unable to allocate memory for table\n");
		return -ENOMEM;
	}
	
	t->name = strdup(name);
	t->numcols = numcols;
	t->refcount = 1;
	t->numrows = 0;
	t->ctx = ctx;

	dbg(ctx, "going to allocate columns...\n");
	va_start(argp, destroy_data);

	for (int i = 0; i < t->numcols; i++)
	{
		GSList *d = va_arg(argp, GSList *);
		if (d == NULL) {
			freeq_table_unref(t);
			err(ctx, "invalid column\n");
			*table = NULL;
			return -1;
		}
		dbg(ctx, "freeq_table_new: adding column %d\n", i);
		t->columns[i].name = strdup(colnames[i]);
		t->columns[i].coltype = coltypes[i];
		t->columns[i].data = d;
		collens[i] = g_slist_length(t->columns[i].data);
	}

	va_end(argp);

	if (ragged(numcols, (int *)&collens, &(t->numrows)))
		dbg(ctx, "freeq_table_new: ragged table detected, using min col length %d\n", t->numrows);

	*table = t;
	return 0;
}

FREEQ_EXPORT int freeq_table_new_fromcols(struct freeq_ctx *ctx,
					  const char *name,
					  int numcols,
					  struct freeq_table **table,
					  bool destroy_data)
{
	va_list argp;
	int collens[numcols];
	struct freeq_table *t;
	struct freeq_column *cols;
	
	t = (struct freeq_table *)malloc(sizeof(struct freeq_table) + (numcols * sizeof(struct freeq_column)));
	if (!t) {
		err(ctx, "unable to allocate memory for table\n");
		return -ENOMEM;
	}
	
	t->name = strdup(name);
	t->numcols = numcols;
	t->numrows = 0;
	t->destroy_data = destroy_data;
	t->refcount = 1;
	t->ctx = ctx;
	*table = t;
	return 0;
}


FREEQ_EXPORT int freeq_table_read(ctx, t, sock)
struct freeq_ctx *ctx;
struct freeq_table **t;
int sock;
{
	union {
		uint32_t i;
		struct longlong s;
	} r;

	char *identity;
	char *name;
	char buf[4096];
	char strbuf[1024] = {0};	
	struct freeq_table *tbl;
	struct freeq_column *cols;
	buffer input;
	int numcols = 0;
	int more = 1;
	int64_t *prev = 0;
      
	buffer_init(&input,read,sock,buf,sizeof buf);

	buffer_getvarint(&input, &(r.s));
	buffer_getn(&input, (char *)&strbuf, (ssize_t)r.i);
	//identity = strndup((char *)&strbuf, r.i);

	buffer_getvarint(&input, &(r.s));
	buffer_getn(&input, (char *)&strbuf, (ssize_t)r.i);
	name = strndup((char *)&strbuf, r.i);

	buffer_getvarint(&input, &(r.s));
	numcols = r.i;

	int err = freeq_table_new_fromcols(ctx,
					   name,
					   numcols,
					   &tbl,
					   true);	
	if (err)
	{
		free(identity);
		free(name);
		return -ENOMEM;
	}

	free(name);
	cols = tbl->columns;

	for (int i = 0; i < numcols; i++)
		buffer_getc(&input, (char *)&(cols[i].coltype));

	for (int i = 0; i < numcols; i++)
	{
		buffer_getvarint(&input, &(r.s));
		buffer_getn(&input, (char *)&strbuf, r.i);
		cols[i].name = strndup((char *)&strbuf, r.i);
	}

	/* you know you're done when the buffer is < buflen dumbass */
	while (more)
	{
		for (int j = 0; j < numcols; j++)
		{
			switch (cols[j].coltype) {
			case FREEQ_COL_STRING:
				buffer_getvarint(&input, &(r.s));
				dezigzag32(&(r.s));
				if (r.i > 0)
				{
					buffer_getn(&input, (char *)&strbuf, r.i);
					cols[j].data = g_slist_prepend(cols[j].data, strndup((char *)&strbuf, r.i));
				}
				else if (r.i < 0)
				{
					dbg(ctx, "cached string index %d\n", r.i);
					cols[j].data = g_slist_prepend(cols[j].data, g_slist_nth_data(cols[j].data, -r.i));
				}
				else
				{
					/* we shouldn't do this, if we
					 * have an empty string we
					 * should just send it once
					 * and then send the offset */
					dbg(ctx, "empty string %d\n", r.i);
				}
				break;
			case FREEQ_COL_NUMBER:
				buffer_getvarint(&input, &(r.s));
				dezigzag32(&(r.s));
				prev = g_slist_nth_data(cols[j].data, r.i);
				cols[j].data = g_slist_prepend(cols[j].data, GINT_TO_POINTER(prev + r.i));
				break;
			case FREEQ_COL_IPV4ADDR:
				break;
			case FREEQ_COL_TIME:
				break;
			default:
				break;
			}
		}
		tbl->numrows++;
		more = 0;
		if (input.p >= input.a)
		{
			int err = buffer_feed(&input);
			if (err)
			{
				more = 0;
			}
		}
	}

	*t = tbl;
	return 0;

}

FREEQ_EXPORT int freeq_table_write(ctx, t, sock)
struct freeq_ctx *ctx;
struct freeq_table *t;
int sock;
{
	int i = 0;
	int c = t->numcols;
	int v, dv;
	int bytes = 0;
	gchar *val;
	uint8_t vibuf[10];
	unsigned short ilen;
	unsigned char slen = 0;
       
	// use a union here
	GHashTable *strtbls[t->numcols];
	int prev[t->numcols];

	GSList *colnxt[t->numcols];

	char buf[4096];
	buffer output;
	buffer_init(&output,write,sock,buf,sizeof buf);

	slen = strlen(ctx->identity);
	buffer_putvarint32(&output, slen);
	buffer_put(&output, (const char *)ctx->identity, slen);

	slen = strlen(t->name);
	buffer_putvarint32(&output, slen);
	buffer_put(&output, (const char *)t->name, slen);

	buffer_putvarint32(&output, c);

	for (i=0; i < c; i++) {
		dbg(ctx, "col %d coltype %d\n", i, t->columns[i].coltype);
		buffer_put(&output, (char *)&(t->columns[i].coltype), sizeof(freeq_coltype_t));
	}

	for (i=0; i < c; i++)
	{
		slen = strlen(t->columns[i].name);
		dbg(ctx, "put slen %d name %s\n", slen, t->columns[i].name);
		buffer_putvarint32(&output, slen);
		buffer_put(&output, (const char *)t->columns[i].name, slen);
	}

	for (i=0; i < c; i++)
	{
		if (t->columns[i].coltype == FREEQ_COL_STRING)
			strtbls[i] = g_hash_table_new_full(g_str_hash,
							   g_str_equal,
							   NULL,
							   NULL);
		colnxt[i] = t->columns[i].data;
	}

	for (i = 0; i < t->numrows; i++) {
		for (int j = 0; j < c; j++)
		{
			dbg(ctx, "row %d col %d type %d\n", i, j, t->columns[j].coltype);
			switch (t->columns[j].coltype)
			{
			case FREEQ_COL_STRING:			       
				val = colnxt[j]->data;
				slen = strlen(val);
				if ((val == NULL) || (slen == 0)) {
					buffer_putbyte(&output, 0);
					break;
				}
				dbg(ctx, "string %s len %d\n", val, slen);
				gpointer elem = g_hash_table_lookup(strtbls[j], val);
				dbg(ctx, "looking for %s (in %p) %p\n", val, strtbls[j], g_hash_table_lookup(strtbls[j], "one"));
				if (elem == NULL)
				{
					dbg(ctx, "havent seen string %s, adding it at position %d/%d in %p\n", val, i, j, strtbls[j]);
					g_hash_table_insert(strtbls[j], val, GINT_TO_POINTER(i));						
					dbg(ctx, "strtbls[%d] size is %d\n",j, g_hash_table_size(strtbls[j]));
					buffer_putvarint32(&output, slen);
					buffer_put(&output, val, slen);
				}
				else
				{
					int32_t idx = GPOINTER_TO_INT(elem);
					buffer_putvarint(&output, idx - i);
					g_hash_table_replace(strtbls[j], val, GINT_TO_POINTER(idx));
				}
				break;				
			case FREEQ_COL_NUMBER:
				v = GPOINTER_TO_INT(colnxt[j]->data);
				buffer_putvarintsigned(&output, v - prev[j]);
				dbg(ctx, "added %d bytes\n", ilen);
				prev[j] = v;
				break;
			case FREEQ_COL_IPV4ADDR:
				break;
			case FREEQ_COL_TIME:
				break;
			default:
				//dbg(ctx, "coltype %d not yet implemented\n", col->coltype);
				break;
			}
			colnxt[j] = g_slist_next(colnxt[j]);		
		}
	}
	for (i = 0; i < c; i++)
		if (strtbls[i] != NULL)
			g_hash_table_destroy(strtbls[i]);
	buffer_flush(&output);
	return bytes;
}

FREEQ_EXPORT void freeq_table_print(struct freeq_ctx *ctx, struct freeq_table *t, FILE *of)
{
	GSList *colp[t->numcols];
       	
	const char *coltypes[] = { "null",
				   "string",
				   "number",
				   "time",
				   "ipv4_addr",
				   "ipv6_addr" };

	memset(colp, 0, t->numcols * sizeof(GSList *) );
	for (int j = 0; j < t->numcols; j++)
	{
		fprintf(stderr, "FUUUUUUUUCK\n");
		colp[j] = t->columns[j].data;
	}
	fprintf(stderr, "colp[0] = %p\n", colp[0]);
	fprintf(stderr, "colp[1] = %p\n", colp[1]);
	//fprintf(of, "%s\n", t->identity);
	fprintf(of, "%s\n", t->name);

	for (int j=0; j < t->numcols; j++)
		fprintf(of, "%s%s", t->columns[j].name, j < t->numcols - 1 ? ", " : "\n");

	for (int j=0; j < t->numcols; j++)
		fprintf(of, "%s%s", coltypes[t->columns[j].coltype], j < t->numcols - 1 ? ", " : "\n");

	for (int i=0; i < t->numrows; i++)
	{
		for (int j=0; j < t->numcols; j++)
		{
			switch (t->columns[j].coltype)
			{
			case FREEQ_COL_STRING:
				fprintf(of, "%p/%s", colp[j], colp[j]->data);
				break;
			case FREEQ_COL_NUMBER:
				fprintf(of, "%p/%d", colp[j], GPOINTER_TO_INT(colp[j]->data));
				break;
			default:
				break;
			}
			colp[j] = g_slist_next(colp[j]);
		}
		fprintf(of, "\n");
	}
}
