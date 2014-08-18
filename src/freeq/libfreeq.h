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
#include "msgpack.h"
#include <glib.h>

/*
 * freeq_ctx
 *
 * library user context - reads the config and system
 * environment, user variables, allows custom logging
 */
struct freeq_ctx;
struct freeq_table_header;
struct freeq_ctx *freeq_ref(struct freeq_ctx *ctx);
struct freeq_ctx *freeq_unref(struct freeq_ctx *ctx);
struct freeq_table_header *freeq_table_header_unref(struct freeq_ctx *ctx, struct freeq_table_header *header);
struct freeq_cbuf;

int freeq_new(struct freeq_ctx **ctx, const char *appname, const char *identity);
void freeq_set_log_fn(struct freeq_ctx *ctx,
		  void (*log_fn)(struct freeq_ctx *ctx,
				 int priority, const char *file, int line, const char *fn,
				 const char *format, va_list args));
int freeq_get_log_priority(struct freeq_ctx *ctx);
void freeq_set_log_priority(struct freeq_ctx *ctx, int priority);
const char *freeq_get_identity(struct freeq_ctx *ctx);
void freeq_set_identity(struct freeq_ctx *ctx, const char *identity);

uint64_t read_varint64(int sock);
uint8_t send_varint64(int sock, uint64_t value);

/*
 * freeq_list
 *
 * access to freeq generated lists
 */

typedef enum
{
	FREEQ_COL_NULL,
	FREEQ_COL_STRING,
	FREEQ_COL_NUMBER,
	FREEQ_COL_TIME,
	FREEQ_COL_IPV4ADDR,
	FREEQ_COL_IPV6ADDR,
} freeq_coltype_t;
	
struct freeq_column {
	freeq_coltype_t coltype;
	char *name;
	GSList *data;
};

struct freeq_table {	
	struct freeq_ctx *ctx;
	int refcount;
	int numrows;
	const char *name;
	int numcols;
	struct freeq_table *next;
	struct freeq_column columns[];
};

void freeq_cbuf_write(struct freeq_cbuf *b, 
		      void *d,
		      ssize_t len);

void freeq_table_print(struct freeq_ctx *ctx,
		       struct freeq_table *table,
		       FILE *f);

int freeq_table_column_new(struct freeq_ctx *ctx,
			   struct freeq_table *table,
			   const char *name,
			   freeq_coltype_t coltype,
			   void *data,
			   size_t len);

/* int freeq_table_column_new_empty(struct freeq_ctx *ctx, */
/* 				 struct freeq_table *table, */
/* 				 const char *name, */
/* 				 freeq_coltype_t coltype, */
/* 				 struct freeq_column **colp, */
/* 				 size_t len); */

//int freeq_attach_all_segments(struct freeq_column *from, struct freeq_column *to);
/* struct freeq_column *freeq_column_get_next(struct freeq_column *column); */
/* struct freeq_column *freeq_column_unref(struct freeq_column *column); */
/* struct freeq_column_segment *freeq_column_segment_unref(struct freeq_column_segment *segment); */
/* const char *freeq_column_get_name(struct freeq_column *column); */
/* const char *freeq_column_get_value(struct freeq_column *column); */
/* #define freeq_column_foreach(column, first_entry) \ */
/* 	for (column = first_entry; \ */
/* 	     column != NULL; \ */
/* 	     column = freeq_column_get_next(column)) */

/*
 * freeq_table
 *
 * access to tables of freeq
 */

struct freeq_table *freeq_table_ref(struct freeq_table *table);
struct freeq_table *freeq_table_unref(struct freeq_table *table);
struct freeq_ctx *freeq_table_get_ctx(struct freeq_table *table);

int freeq_table_send(struct freeq_ctx *c, struct freeq_table *table);
int freeq_table_write(struct freeq_ctx *c, struct freeq_table *table, int sock);
int freeq_table_write_sock(struct freeq_ctx *c, struct freeq_table *table, int sock);
int freeq_error_write_sock(struct freeq_ctx *ctx, const char *errmsg, int sock);
int freeq_table_pack_msgpack(msgpack_sbuffer *sbuf, struct freeq_ctx *ctx, struct freeq_table *table);

int freeq_table_new(struct freeq_ctx *ctx,
		    const char *name,
		    int numcols,
		    freeq_coltype_t coltypes[],
		    const char *colnames[],
		    struct freeq_table **table, ...);

int freeq_table_header_from_msgpack(struct freeq_ctx *ctx, char *buf, size_t bufsize, struct freeq_table **table);
int freeq_table_to_text(struct freeq_ctx *ctx, struct freeq_table *table);
//struct freeq_column *freeq_table_get_some_column(struct freeq_table *table);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
