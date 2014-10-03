/*
  libfreeq - something with freeq

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

#ifndef _LIBFREEQ_H_
#define _LIBFREEQ_H_

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif
#include <glib.h>

#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>

#include "openssl/bio.h"
#include "openssl/ssl.h"
#include "openssl/err.h"


/*
 * freeq_ctx
 *
 * library user context - reads the config and system
 * environment, user variables, allows custom logging
 */
struct freeq_ctx;
struct freeq_ctx *freeq_ref(struct freeq_ctx *ctx);
struct freeq_ctx *freeq_unref(struct freeq_ctx *ctx);
struct freeq_cbuf;

typedef struct freeq_generation_t freeq_generation_t;
struct freeq_generation_t
{
	unsigned int refcount;
	GHashTable *tables;
	GStringChunk *strings;
	time_t era;
	GRWLock *rw_lock;
	freeq_generation_t *next;
};

int freeq_new(struct freeq_ctx **ctx, const char *appname, const char *identity);
void freeq_set_log_fn(struct freeq_ctx *ctx,
		  void (*log_fn)(struct freeq_ctx *ctx,
				 int priority, const char *file, int line, const char *fn,
				 const char *format, va_list args));
int freeq_get_log_priority(struct freeq_ctx *ctx);
void freeq_set_log_priority(struct freeq_ctx *ctx, int priority);
const char *freeq_get_identity(struct freeq_ctx *ctx);
void freeq_set_identity(struct freeq_ctx *ctx, const char *identity);
int freeq_generation_new(freeq_generation_t *gen);
/*
 * freeq_list
 *
 * access to freeq generated lists
 */

typedef uint8_t freeq_coltype_t;
#define	FREEQ_COL_NULL 0
#define	FREEQ_COL_STRING 1
#define FREEQ_COL_NUMBER 2
#define FREEQ_COL_TIME 3
#define FREEQ_COL_IPV4ADDR 4
#define FREEQ_COL_IPV6ADDR 5

/* typedef enum */
/* { */
/*	FREEQ_COL_NULL, */
/*	FREEQ_COL_STRING, */
/*	FREEQ_COL_NUMBER, */
/*	FREEQ_COL_TIME, */
/*	FREEQ_COL_IPV4ADDR, */
/*	FREEQ_COL_IPV6ADDR, */
/* } freeq_coltype_t; */

struct freeq_column {
	freeq_coltype_t coltype;
	char *name;
	GSList *data;
};

struct freeq_table {
	struct freeq_ctx *ctx;
	int refcount;
	int numrows;
	char *name;
	char *identity;
	int numcols;
	bool destroy_data;
	GStringChunk *strings;
	GHashTable *senders;
	GRWLock *rw_lock;
	struct freeq_table *next;
	struct freeq_column columns[];
};

/*
options for table merging

can't lock the table for the entire time we're reading
can't really insert into the table's gstrchunk without locking it
what if the context had a gstrchunk?
the generation could have a gstrchunk, if we exposed a function that let you specify the table's gstrchunk!
does gstrchunk need to be locked?
in this scenario, all strings are allocated out of the generation, so when the generation is freed the memory is released
this means we can append two tables without having to copy strchunks
*/

void freeq_table_print(struct freeq_ctx *ctx,
		       struct freeq_table *table,
		       FILE *of);

int freeq_table_column_new(struct freeq_ctx *ctx,
			   struct freeq_table *table,
			   const char *name,
			   freeq_coltype_t coltype,
			   void *data,
			   size_t len);

/* int freeq_table_column_new_empty(struct freeq_ctx *ctx, */
/*				 struct freeq_table *table, */
/*				 const char *name, */
/*				 freeq_coltype_t coltype, */
/*				 struct freeq_column **colp, */
/*				 size_t len); */

//int freeq_attach_all_segments(struct freeq_column *from, struct freeq_column *to);
/* struct freeq_column *freeq_column_get_next(struct freeq_column *column); */
/* struct freeq_column *freeq_column_unref(struct freeq_column *column); */
/* struct freeq_column_segment *freeq_column_segment_unref(struct freeq_column_segment *segment); */
/* const char *freeq_column_get_name(struct freeq_column *column); */
/* const char *freeq_column_get_value(struct freeq_column *column); */
/* #define freeq_column_foreach(column, first_entry) \ */
/*	for (column = first_entry; \ */
/*	     column != NULL; \ */
/*	     column = freeq_column_get_next(column)) */

/*
 * freeq_table
 *
 * access to tables of freeq
 */

struct freeq_table *freeq_table_ref(struct freeq_table *table);
struct freeq_table *freeq_table_unref(struct freeq_table *table);
struct freeq_ctx *freeq_table_get_ctx(struct freeq_table *table);

int freeq_table_write(struct freeq_ctx *c, struct freeq_table *table, int sock);
int freeq_table_bio_write(struct freeq_ctx *c, struct freeq_table *table, BIO *b);
int freeq_table_read(struct freeq_ctx *c, struct freeq_table **table, int sock);
int freeq_table_bio_read(struct freeq_ctx *c, struct freeq_table **table, BIO *b, GStringChunk *strchunk);
int freeq_table_bio_read_header(struct freeq_ctx *ctx, struct freeq_table **t, BIO *b);
int freeq_table_bio_read_tabledata(struct freeq_ctx *ctx, struct freeq_table *t, BIO *b, GStringChunk *strchnk);

int freeq_table_ssl_read(struct freeq_ctx *ctx, struct freeq_table **tbl, SSL *ssl);
int freeq_table_sendto_ssl(struct freeq_ctx *freeqctx, struct freeq_table *t);

int freeq_table_new(struct freeq_ctx *ctx,
		    const char *name,
		    int numcols,
		    freeq_coltype_t coltypes[],
		    const char *colnames[],
		    struct freeq_table **table,
		    bool destroy_data,
		    ...);

int freeq_table_new_fromcols(struct freeq_ctx *ctx,
			     const char *name,
			     int numcols,
			     struct freeq_table **table,
			     GStringChunk *strchnk,
			     bool destroy_data);

int freeq_table_header_from_msgpack(struct freeq_ctx *ctx, char *buf, size_t bufsize, struct freeq_table **table);
int freeq_table_to_text(struct freeq_ctx *ctx, struct freeq_table *table);
//struct freeq_column *freeq_table_get_some_column(struct freeq_table *table);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
