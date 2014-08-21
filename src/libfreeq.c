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

	if (table->next != NULL)
		freeq_table_unref(table->next);

	table->refcount--;
	if (table->refcount > 0)
		return NULL;

	//for (int i=0; i < table->numcols; i++)
	//	freeq_column_unref(&(table->columns[i]));

	//dbg(table->ctx, "context %p released\n", table);
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

FREEQ_EXPORT void freeq_table_print(struct freeq_ctx *ctx,
				    struct freeq_table *t,
				    FILE *f)
{
	fprintf(f, "name: %s\n", t->name);
	for (int j = 0; j < t->numcols; j++) {
		fprintf(f, "%s", t->columns[j].name);
		fprintf(f, j < (t->numcols - 1) ? "," : "\n");
	}

	dbg(ctx, "numrows: %d\n", t->numrows);
	for (int i = 0; i < t->numrows; i++) {
		for (int j = 0; j < t->numcols; j++) {
			switch (t->columns[j].coltype)
			{
			case FREEQ_COL_STRING:
				fprintf(f, "%s", g_slist_nth_data(t->columns[j].data, i));
				break;
			case FREEQ_COL_NUMBER:
				fprintf(f, "%d", g_slist_nth_data(t->columns[j].data, i));
				break;
			default:
				break;
			}
			fprintf(f, j < (t->numcols - 1) ? "," : "\n");
		}
	}
}

bool ragged(int c, int rlens[], int *min) {
	*min = rlens[0];
	fprintf(stderr, "min set to %d\n", *min);
	bool ragged = false;

	for (int i = 0; i < c; i++) {
		fprintf(stderr, "len of col %d is %d\n", i, rlens[i]);
		if (rlens[i] < *min) {
			fprintf(stderr, "adjusted min to %d\n", rlens[i]);
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
				 ...)
{
	va_list argp;
	int collens[numcols];
	struct freeq_table *t;
	t = (struct freeq_table *)malloc(sizeof(struct freeq_table) + sizeof(struct freeq_column) * numcols);

	if (!t) {
		err(ctx, "unable to allocate memory for table\n");
		return -ENOMEM;
	}
	t->name = name;
	t->numcols = numcols;
	t->refcount = 1;
	t->ctx = ctx;

	dbg(ctx, "going to allocate columns...\n");
	va_start(argp, table);

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

void free_data(gpointer d)
{
	g_free(d);
}

int bsz(int num) {
	int n = 0;
	if (num > 0) {
		while (num != 0) {
			num >>= 8;
			n++;
		}
	}
	return n;
}

FREEQ_EXPORT int freeq_table_read(ctx, t, sock)
struct freeq_ctx *ctx;
struct freeq_table **t;
int sock;
{
	union {		
		uint32_t i;
		struct longlong s;	
	} result;

	char buf[4096];
	char strbuf[1024];	
	buffer input;
	buffer_init(&input,read,sock,buf,sizeof buf);
	fprintf(stderr, "allocated a buffer of size %d\n", sizeof(buf));
	
	buffer_getvarint(&input, &(result.s)); 
	buffer_getn(&input, (char *)&strbuf, (ssize_t)result.i);

	fprintf(stderr, "identity: %s\n", (char *)&strbuf); 
	
}

FREEQ_EXPORT int freeq_table_write(ctx, t, sock)
struct freeq_ctx *ctx;
struct freeq_table *t;
int sock;
{
	int i = 0;
	int c = t->numcols;
	int v, dv;
	int b = 0;
	gchar *val;
	uint8_t vibuf[10];
	unsigned short ilen;
	unsigned char slen = 0;
	int bytes = 0, dbytes = 0;
	// use a union here
	GHashTable *strtbls[t->numcols];
	int prev[t->numcols];

	char buf[4096];
	buffer output;
	buffer_init(&output,write,sock,buf,sizeof buf);

	slen = strlen(ctx->identity) + 1;
	buffer_put(&output, &slen, sizeof(slen));
	buffer_puts(&output, (const char *)ctx->identity);

	slen = strlen(t->name) + 1;
	buffer_put(&output, &slen, sizeof(slen));
	buffer_puts(&output, (const char *)t->name);

	buffer_put(&output, (char *)&c, sizeof(c));

	for (i=0; i < c; i++)
		buffer_put(&output, (char *)&(t->columns[i].coltype), sizeof(freeq_coltype_t));

	for (i=0; i < c; i++)
	{
		slen = strlen(t->columns[i].name) + 1;
		buffer_put(&output, &slen, sizeof(slen));
		buffer_puts(&output, (const char *)t->columns[i].name);
	}

	for (i=0; i < c; i++)
	{
		if (t->columns[i].coltype == FREEQ_COL_STRING)
			strtbls[i] = g_hash_table_new_full(g_str_hash,
							   g_str_equal,
							   NULL,
							   g_free);
	}

	for (i = 0; i < t->numrows; i++) {
		fprintf(stderr, "row %d\n", i);
		for (int j = 0; j < c; j++)
		{
			fprintf(stderr, "col %d\n", j);
			switch (t->columns[j].coltype)
			{
			case FREEQ_COL_STRING:
				val = g_slist_nth_data(t->columns[j].data, i);
				int32_t len = strlen(val);
				fprintf(stderr, "string %s len %d\n", val, len);
				if (len > 0)
				{
					gpointer elem = g_hash_table_lookup(strtbls[j], val);
					fprintf(stderr, "looking for %s (in %p) %p\n", val, strtbls[j], g_hash_table_lookup(strtbls[j], "one"));
					if (elem == NULL)
					{
						fprintf(stderr, "havent seen string %s, adding it at position %d/%d in %p\n", val, i, j, strtbls[j]);
						int32_t *idx = malloc(sizeof(int));
						*idx = i;
						g_hash_table_insert(strtbls[j], val, idx);
						fprintf(stderr, "strtbls[%d] size is %d\n",j, g_hash_table_size(strtbls[j]));
						buffer_put(&output, (const char *)&len, sizeof(len));
						buffer_puts(&output, val);
					}
					else
					{
						int32_t idx = *(int32_t *)elem;
						ilen = _pbcV_zigzag32(-idx, vibuf);
						buffer_put(&output, (const char *)&vibuf, ilen);
						fprintf(stderr, "we already saw %s so we're sending %d\n", val, -idx);
					}
				}
				else
				{
					fprintf(stderr, "empty string, sending a null\n", val, len);
					b += write(sock, 0, 1);
				}
				break;

			case FREEQ_COL_NUMBER:
				v = GPOINTER_TO_INT(g_slist_nth_data(t->columns[j].data, i));
				dv = abs(v - prev[j]);
				ilen = _pbcV_encode(dv, vibuf);
				buffer_put(&output, (const char *)&vibuf, ilen);
				fprintf(stderr, "added %d bytes\n", ilen);
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
		}
	}
	buffer_flush(&output);

	fprintf(stderr, "raw %d compressed %d bytes\n", bytes, dbytes);
	return b;
}


/* FREEQ_EXPORT int freeq_table_to_text(struct freeq_ctx *ctx, struct freeq_table *table) */
/* { */
/*	const char **strarrp = NULL; */
/*	int *intarrp = NULL; */
/*	struct freeq_column_segment *seg; */
/*	struct freeq_column *colp = table->columns; */

/*	while (colp != NULL)  */
/*	{ */
/*		printf("%s", colp->name); */
/*		colp = colp->next; */
/*		if (colp != NULL) */
/*			printf(", "); */
/*	} */
/*	printf("\n"); */

/*	for (unsigned i = 0; i < table->numrows; i++) */
/*	{ */
/*		colp = table->columns; */
/*		while (colp != NULL)  */
/*		{ */
/*			seg = colp->segments; */
/*			strarrp = (const char **)seg->data; */
/*			intarrp = (int *)seg->data; */
/*			switch (colp->coltype) */
/*			{ */
/*			case FREEQ_COL_STRING: */
/*				printf("%s", strarrp[i]); */
/*				break; */
/*			case FREEQ_COL_NUMBER: */
/*				printf("%d", intarrp[i]); */
/*				break; */
/*			default: */
/*				break; */
/*			} */
/*			if (colp != NULL) */
/*				printf(", "); */
/*			colp = colp->next; */
/*		} */
/*		printf("\n"); */
/*	} */
/* } */
