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
#include <inttypes.h>

#include <freeq/libfreeq.h>
#include "libfreeq-private.h"

#include <nanomsg/nn.h>
#include <nanomsg/pipeline.h>

#include "varint.h"
#include "ssl-common.h"
#include "openssl/bio.h"
#include "openssl/ssl.h"
#include "openssl/err.h"

#define CSEP(j, t) j < t->numcols - 1 ? ", " : "\n"

const char *coltypes[] = { "null",
			   "string",
			   "number",
			   "time",
			   "ipv4_addr",
			   "ipv6_addr" };

int BIO_write_varintsigned32(BIO *b, uint32_t number);
int BIO_write_varint32(BIO *b, uint32_t number);
int BIO_write_varintsigned(BIO *b, uint32_t number);
ssize_t BIO_read_varint(BIO *b, struct longlong *result);
unsigned int bio_wrap(struct freeq_ctx *ctx, struct freeq_table *tbl, SSL *ssl);

static bool ssl_initialized;
struct CRYPTO_dynlock_value
{
    pthread_mutex_t mutex;
};

static pthread_mutex_t *mutex_buf = NULL;
DH *dh512 = NULL;
DH *dh1024 = NULL;


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
	const char* appname;
	SSL_CTX *sslctx;
	int log_priority;
};

typedef struct {
	char vibuf[10];
	char vibuf32[5];
} vibuf_t;

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

void
dezigzag64(struct longlong *r)
{
	uint32_t low = r->low;
	r->low = ((low >> 1) | ((r->hi & 1) << 31)) ^ - (low & 1);
	r->hi = (r->hi >> 1) ^ - (low & 1);
}

void
dezigzag32(struct longlong *r)
{
	uint32_t low = r->low;
	r->low = (low >> 1) ^ - (low & 1);
	r->hi = -(low >> 31);
}


/* This code was shamelessly stolen and adapted from beautiful C-only
   version of protobuf by 云风 (cloudwu) , available at:
   https://github.com/cloudwu/pbc */

typedef struct {
	char data[5];
} varint32_buf_t;

typedef struct {
	char data[10];
} varint_buf_t;

static inline int
encode_varint32(varint32_buf_t *b, uint32_t number)
{
	if (number < 0x80) {
		b->data[0] = (uint8_t) number;
		return 1;
	}
	b->data[0] = (uint8_t) (number | 0x80 );
	if (number < 0x4000) {
		b->data[1] = (uint8_t) (number >> 7 );
		return 2;
	}
	b->data[1] = (uint8_t) ((number >> 7) | 0x80 );
	if (number < 0x200000) {
		b->data[2] = (uint8_t) (number >> 14);
		return 3;
	}
	b->data[2] = (uint8_t) ((number >> 14) | 0x80 );
	if (number < 0x10000000) {
		b->data[3] = (uint8_t) (number >> 21);
		return 4;
	}
	b->data[3] = (uint8_t) ((number >> 21) | 0x80 );
	b->data[4] = (uint8_t) (number >> 28);
	return 5;
}

static inline int
encode_varint(varint_buf_t *b, uint64_t number)
{
	if ((number & 0xffffffff) == number) {
		return encode_varint32((varint32_buf_t *)b, (uint32_t)number);
	}
	int i = 0;
	do {
		b->data[i] = (uint8_t)(number | 0x80);
		//buffer_putbyte(b, (uint8_t)(number | 0x80));
		number >>= 7;
		++i;
	} while (number >= 0x80);
	b->data[i] = (uint8_t)number;
	//buffer_putbyte(b, (uint8_t)number);
	return i+1;
}

static inline int
encode_varintsigned32(varint32_buf_t *buffer, int32_t n)
{
	n = (n << 1) ^ (n >> 31);
	return encode_varint32(buffer,n);
}

static inline int
encode_varintsigned(varint_buf_t *buffer, int64_t n)
{
	n = (n << 1) ^ (n >> 63);
	return encode_varint(buffer,n);
}

int
BIO_write_varint32(BIO *b, uint32_t number)
{
	varint32_buf_t buf;
	int len = encode_varint32(&buf, number);
	BIO_write(b, &buf, len);
}

int
BIO_write_varint(BIO *b, uint32_t number)
{
	varint_buf_t buf;
	int len = encode_varint(&buf, number);
	BIO_write(b, &buf, len);
}

int
BIO_write_varintsigned32(BIO *b, uint32_t number)
{
	varint32_buf_t buf;
	int len = encode_varintsigned32(&buf, number);
	BIO_write(b, &buf, len);
}

int
BIO_write_varintsigned(BIO *b, uint32_t number)
{
	varint_buf_t buf;
	int len = encode_varintsigned(&buf, number);
	BIO_write(b, &buf, len);
}

ssize_t
BIO_read_varint(BIO *b, struct longlong *result) {
	char x;

	if (!BIO_read(b, &x, 1))
		return 0;

	if (!(x & 0x80)) {
		result->low = x;
		result->hi = 0;
		return 1;
	}
	uint32_t r = x & 0x7f;
	int i;
	for (i=1;i<4;i++) {
		BIO_read(b, &x, 1);
		r |= ((x&0x7f) << (7*i));
		if (!(x & 0x80)) {
			result->low = r;
			result->hi = 0;
			return i+1;
		}
	}
	uint64_t lr = 0;
	for (i=4;i<10;i++) {
		BIO_read(b, &x, 1);
		lr |= ((uint64_t)(x & 0x7f) << (7*(i-4)));
		if (!(x & 0x80)) {
			result->hi = (uint32_t)(lr >> 4);
			result->low = r | (((uint32_t)lr & 0xf) << 28);
			return i+1;
		}
	}
	result->low = 0;
	result->hi = 0;
	return 10;
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

void init_dhparams(void) {
    BIO *bio;
    bio = BIO_new_file("control/dh512.pem", "r");
    if (!bio)
        int_error("Error opening file dh512.pem");
    dh512 = PEM_read_bio_DHparams(bio, NULL, NULL, NULL);
    if (!dh512)
        int_error("Error reading DH parameters from dh512.pem");
    BIO_free(bio);
    bio = BIO_new_file("control/dh1024.pem", "r");
    if (!bio)
        int_error("Error opening file dh1024.pem");
    dh1024 = PEM_read_bio_DHparams(bio, NULL, NULL, NULL);
    if (!dh1024)
        int_error("Error reading DH parameters from dh1024.pem");
    BIO_free(bio);
}
DH *tmp_dh_callback(SSL *ssl, int is_export, int keylength)
{
    DH *ret;
    if (!dh512 || !dh1024)
        init_dhparams();

    switch (keylength) {
    case 512:
        ret = dh512;
        break;
    case 1024:
    default:
        ret = dh1024;
        break;
    }
    return ret;
}

static struct CRYPTO_dynlock_value * dyn_create_function(const char *file,
                                                         int line)
{
    struct CRYPTO_dynlock_value *value;
    value = (struct CRYPTO_dynlock_value *)malloc(sizeof(struct CRYPTO_dynlock_value));
    if (!value)
        return NULL;

    pthread_mutex_init(&(value->mutex), NULL);
    return value;
}
static void dyn_lock_function(int mode, 
                              struct CRYPTO_dynlock_value *l,
                              const char *file, 
                              int line)
{
    if (mode & CRYPTO_LOCK)
        pthread_mutex_lock(&(l->mutex));
    else
        pthread_mutex_unlock(&(l->mutex));
}
static void dyn_destroy_function(struct CRYPTO_dynlock_value *l,
                                 const char *file, int line)
{
    pthread_mutex_destroy(&(l->mutex));
    free(l);
}

SSL_CTX *setup_client_ctx(void)
{
    SSL_CTX *ctx;
 
    ctx = SSL_CTX_new(SSLv23_method(  ));
    if (SSL_CTX_load_verify_locations(ctx, CAFILE, CADIR) != 1)
        int_error("Error loading CA file and/or directory");
    if (SSL_CTX_set_default_verify_paths(ctx) != 1)
        int_error("Error loading default CA file and/or directory");
    if (SSL_CTX_use_certificate_chain_file(ctx, CERTFILE) != 1)
        int_error("Error loading certificate from file");
    if (SSL_CTX_use_PrivateKey_file(ctx, CERTFILE, SSL_FILETYPE_PEM) != 1)
        int_error("Error loading private key from file");
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, verify_callback);
    SSL_CTX_set_verify_depth(ctx, 4);
    SSL_CTX_set_options(ctx, SSL_OP_ALL|SSL_OP_NO_SSLv2);
    if (SSL_CTX_set_cipher_list(ctx, CIPHER_LIST) != 1)
        int_error("Error setting cipher list (no valid ciphers)");
    return ctx;
}

static void locking_function(int mode, int n, const char * file, int line)
{
    if (mode & CRYPTO_LOCK)
        pthread_mutex_lock(&(mutex_buf[n]));
    else
        pthread_mutex_unlock(&(mutex_buf[n]));
}

static unsigned long id_function(void)
{
    return ((unsigned long)pthread_self());
}

int freeq_init_ssl(struct freeq_ctx *ctx) 
{
	if (ssl_initialized)
		return true;
	
	int i;
	mutex_buf = (pthread_mutex_t *)malloc(CRYPTO_num_locks() *sizeof(pthread_mutex_t));
	if (!mutex_buf)
		exit(1);
	
	for (i = 0; i < CRYPTO_num_locks(); i++)
		pthread_mutex_init(&(mutex_buf[i]), NULL) ;
	
	CRYPTO_set_id_callback(id_function);
	CRYPTO_set_locking_callback(locking_function);
	CRYPTO_set_dynlock_create_callback(dyn_create_function);
	CRYPTO_set_dynlock_lock_callback(dyn_lock_function);
	CRYPTO_set_dynlock_destroy_callback(dyn_destroy_function);
	
	SSL_library_init();
	SSL_load_error_strings();    
	seed_prng();		
	ctx->sslctx = setup_client_ctx();
	ssl_initialized = true;	
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
			g_slist_free(table->columns[i].data);			
//			if (table->columns[i].coltype == FREEQ_COL_STRING)
//			{
//				g_slist_free_full(table->columns[i].data, g_free);
//			}
//			else				
		}
		if (table->strchunk != NULL)
			g_string_chunk_free(table->strchunk);
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

FREEQ_EXPORT int freeq_error_write_sock(struct freeq_ctx *ctx, const char *errmsg, BIO *b)
{
	struct freeq_table errtbl;

	//freeq_coltype_t coltypes[] = { FREEQ_COL_STRING };
	//char *colnames[] = { "error" };

	errtbl.name = "error";
	errtbl.numcols = 1;
	errtbl.numrows = 1;

	dbg(ctx, "generated error table, sending...\n");
	return freeq_table_bio_write(ctx, &errtbl, b);
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
	struct freeq_table *t;
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

FREEQ_EXPORT int freeq_table_bio_read(ctx, t, b)
struct freeq_ctx *ctx;
struct freeq_table **t;
BIO *b;
{
	union {
		int64_t i;
		struct longlong s;
	} r;

	char *identity;
	char *name;
	char strbuf[1024] = {0};
	struct freeq_table *tbl;
	struct freeq_column *cols;
	ssize_t read;
	int numcols = 0;
	int more = 1;
	int slen = 0;
	unsigned int rb = 0;

	rb += BIO_read_varint(b, &(r.s));
	rb += BIO_read(b, (char *)&strbuf, (ssize_t)r.i);
	name = strndup((char *)&strbuf, r.i);
	dbg(ctx, "name %s read %d\n", name, rb);

	rb += BIO_read_varint(b, &(r.s));
	numcols = r.i;
	dbg(ctx, "numcols %d read %d\n", numcols, rb);

	int64_t prev[r.i];
	memset(prev, 0, numcols * sizeof(int64_t));

	int err = freeq_table_new_fromcols(ctx,
					   name,
					   numcols,
					   &tbl,
					   true);
	if (err)
	{
		free(identity);
		free(name);
		free(prev);
		return -ENOMEM;
	}

	free(name);
	cols = tbl->columns;

	for (int i = 0; i < numcols; i++)
	{
		rb += BIO_read(b, (char *)&(cols[i].coltype), 1);
	}

	tbl->strchunk = g_string_chunk_new(8);
	
	dbg(ctx, "coltypes, read %d\n", rb);

	for (int i = 0; i < numcols; i++)
	{
		rb += BIO_read_varint(b, &(r.s));
		rb += BIO_read(b, (char *)&strbuf, r.i);
		cols[i].name = strndup((char *)&strbuf, r.i);
	}
	dbg(ctx, "colnames, read %d\n", rb);

	int i = 0;
	/* you know you're done when the buffer is < buflen dumbass */
	while (more)
	{
		for (int j = 0; j < numcols; j++)
		{
			r.i = 0;
			read = BIO_read_varint(b, &(r.s));
			if (read == 0)
			{
				more = 0;
				break;
			}
			rb += read;
			switch (cols[j].coltype) {
			case FREEQ_COL_STRING:
				dezigzag32(&(r.s));
				slen = r.i;
				dbg(ctx, "%d/%d len %d read %d\n",i,j, r.i, rb);			
				if (slen > 0)
				{
					rb += BIO_read(b, (char *)&strbuf, slen);
					strbuf[slen] = 0;
					//cols[j].data = g_slist_prepend(cols[j].data, strndup((char *)&strbuf, slen));
					cols[j].data = g_slist_prepend(cols[j].data, g_string_chunk_insert_const(tbl->strchunk, (char *)&strbuf));
				}
				else if (slen < 0)
				{
					dbg(ctx, "negative offset %d list length is %d, value at offset is %s\n", slen, g_slist_length(cols[j].data), g_slist_nth_data(cols[j].data, -slen -1));
					cols[j].data = g_slist_prepend(cols[j].data, g_slist_nth_data(cols[j].data, -slen -1));
				}
				else
				{
					/* we shouldn't do this, if we
					 * have an empty string we
					 * should just send it once
					 * and then send the offset */
					dbg(ctx, "%d/%d empty string %d\n",i,j,slen);
					cols[j].data = g_slist_prepend(cols[j].data, NULL);
					//dbg(ctx, "string %s read %d\n", cols[j].data->data, buf.p);
				}
				dbg(ctx, "%d/%d val %s\n",i,j, cols[j].data->data);					
				break;
			case FREEQ_COL_NUMBER:
				dezigzag64(&(r.s));
				dbg(ctx, "%d/%d value raw %" PRId64 " delta %" PRId64 " read %d\n", i,j, r.i, prev[j] + r.i, rb);
				prev[j] = prev[j] + r.i;
				cols[j].data = g_slist_prepend(cols[j].data, GINT_TO_POINTER(prev[j]));
				dbg(ctx, "%d/%d val %d\n",i,j, cols[j].data->data);					
				break;
			case FREEQ_COL_IPV4ADDR:
				break;
			case FREEQ_COL_TIME:
				break;
			default:
				break;
			}
		}
		if (more)
			i++;
	}

	for (int i = 0; i < numcols; i++)
		cols[i].data = g_slist_reverse(cols[i].data);

	dbg(ctx, "READ %d rows\n", i);
	tbl->numrows = i;
	*t = tbl;
	return 0;
}

unsigned int bio_wrap(struct freeq_ctx *ctx, struct freeq_table *tbl, SSL *ssl)
{
	BIO  *buf_io, *ssl_bio;		

	buf_io = BIO_new(BIO_f_buffer());
	ssl_bio = BIO_new(BIO_f_ssl());
	BIO_set_ssl(ssl_bio, ssl, BIO_CLOSE);
	BIO_push(buf_io, ssl_bio);
	
	return freeq_table_bio_write(ctx, tbl, buf_io);       	
}

FREEQ_EXPORT int freeq_table_sendto_ssl(struct freeq_ctx *freeqctx, struct freeq_table *t)
{
	BIO     *conn;
	SSL     *ssl;	
	long    err;
	const char *server = "localhost";

	if (freeq_init_ssl(freeqctx))
		exit(1);
			
	conn = BIO_new_connect("localhost:13000");
	if (!conn)
		int_error("Error creating connection BIO") ;
    
	if (BIO_do_connect(conn) <= 0)
		int_error("Error connecting to remote machine");
	
	ssl = SSL_new(freeqctx->sslctx);
	SSL_set_bio(ssl, conn, conn);
	if (SSL_connect(ssl) <= 0)
		int_error("Error connecting SSL object");
	if ((err = post_connection_check(ssl, server)) != X509_V_OK)
	{
		fprintf(stderr, "-Error: peer certificate: %s\n",
			X509_verify_cert_error_string(err));
		int_error("Error checking SSL object after connection");
	}
	fprintf(stderr, "SSL Connection opened\n");
	if (bio_wrap(freeqctx, t, ssl))
		SSL_shutdown(ssl);
	else
		SSL_clear(ssl);
	fprintf(stderr, "SSL Connection closed\n");
	
	SSL_free(ssl);

	//SSL_CTX_free(sslctx);
	return 0;
	
}

FREEQ_EXPORT int freeq_table_bio_write(ctx, t, b)
struct freeq_ctx *ctx;
struct freeq_table *t;
BIO *b;
{
	int i = 0;
	int c = t->numcols;
	gchar *val;
	int slen = 0;
	unsigned int wb = 0;

	// use a union here
	GHashTable *strtbls[t->numcols];
	uint64_t prev[t->numcols];
	memset(prev, t->numcols, 0);
	GSList *colnxt[t->numcols];

	//char bufalloc[4096];
	//buffer buf;
	//buffer_init(&buf,write,sock,bufalloc,sizeof bufalloc);

	slen = strlen(t->name);
	wb += BIO_write_varint32(b, slen);
	wb += BIO_write(b, (const char *)t->name, slen);
	wb += BIO_write_varint32(b, c);

	for (i=0; i < c; i++)
		wb += BIO_write(b, (char *)&(t->columns[i].coltype), sizeof(freeq_coltype_t));

	for (i=0; i < c; i++)
	{
		slen = strlen(t->columns[i].name);
		wb += BIO_write_varint32(b, slen);
		wb += BIO_write(b, (const char *)t->columns[i].name, slen);
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
			uint64_t num = 0;
			switch (t->columns[j].coltype)
			{
			case FREEQ_COL_STRING:
				val = colnxt[j]->data;
				slen = strlen(val);
				if ((val == NULL) || (slen == 0)) {
					wb += BIO_write(b, 0, 1);
					dbg(ctx, "%d/%d string empty\n", i,j);
					break;
				}

				if (g_hash_table_contains(strtbls[j], val))
				{
					unsigned int idx = GPOINTER_TO_INT(g_hash_table_lookup(strtbls[j], val));
					slen = idx - i;
					wb += BIO_write_varintsigned32(b, slen);
					dbg(ctx, "%d/%d str %s len %d write %d\n",i,j,val,slen, wb);
					g_hash_table_replace(strtbls[j], val, GINT_TO_POINTER(i));
				}
				else
				{
					g_hash_table_insert(strtbls[j], val, GINT_TO_POINTER(i));
					wb += BIO_write_varintsigned32(b, slen);
					wb += BIO_write(b, val, slen);
					dbg(ctx, "%d/%d str %s len %d write %d\n", i,j,val, slen, wb);
				}

				break;
			case FREEQ_COL_NUMBER:
				num = GPOINTER_TO_INT(colnxt[j]->data);
				wb += BIO_write_varintsigned(b, (int64_t)num - prev[j]);
				prev[j] = num;
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

	BIO_flush(b);
}


FREEQ_EXPORT void freeq_table_print(struct freeq_ctx *ctx, struct freeq_table *t, FILE *of)
{
	GSList *colp[t->numcols];

	memset(colp, 0, t->numcols * sizeof(GSList *) );
	for (int j = 0; j < t->numcols; j++)
		colp[j] = t->columns[j].data;

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
				fprintf(of, "%s", colp[j]->data);
				break;
			case FREEQ_COL_NUMBER:
				fprintf(of, "%d", GPOINTER_TO_INT(colp[j]->data));
				break;
			default:
				break;
			}
			fprintf(of, CSEP(j, t));
			colp[j] = g_slist_next(colp[j]);
		}
	}
}

FREEQ_EXPORT void handle_error(const char *file, int lineno, const char *msg)
{
    fprintf(stderr, "** %s:%i %s\n", file, lineno, msg);
    ERR_print_errors_fp(stderr);
    //exit(-1);
}

FREEQ_EXPORT int verify_callback(int ok, X509_STORE_CTX *store)
{
    char data[256];
 
    if (!ok)
    {
        X509 *cert = X509_STORE_CTX_get_current_cert(store);
        int  depth = X509_STORE_CTX_get_error_depth(store);
        int  err = X509_STORE_CTX_get_error(store);
 
        fprintf(stderr, "-Error with certificate at depth: %i\n", depth);
        X509_NAME_oneline(X509_get_issuer_name(cert), data, 256);
        fprintf(stderr, "  issuer   = %s\n", data);
        X509_NAME_oneline(X509_get_subject_name(cert), data, 256);
        fprintf(stderr, "  subject  = %s\n", data);
        fprintf(stderr, "  err %i:%s\n", err, X509_verify_cert_error_string(err));
    }
 
    return ok;
}

FREEQ_EXPORT long post_connection_check(SSL *ssl, const char *host)
{
    X509      *cert;
    X509_NAME *subj;
    char      data[256];
    int       extcount;
    int       ok = 0;
 
    /* Checking the return from SSL_get_peer_certificate here is not strictly
     * necessary.  With our example programs, it is not possible for it to return
     * NULL.  However, it is good form to check the return since it can return NULL
     * if the examples are modified to enable anonymous ciphers or for the server
     * to not require a client certificate.
     */
    if (!(cert = SSL_get_peer_certificate(ssl)) || !host)
        goto err_occured;
    if ((extcount = X509_get_ext_count(cert)) > 0)
    {
        int i;
 
        for (i = 0;  i < extcount;  i++)
        {
            char              *extstr;
            X509_EXTENSION    *ext;
 
            ext = X509_get_ext(cert, i);
            extstr = (char*) OBJ_nid2sn(OBJ_obj2nid(X509_EXTENSION_get_object(ext)));
 
            if (!strcmp(extstr, "subjectAltName"))
            {
                int                  j;
                unsigned char        *data;
                STACK_OF(CONF_VALUE) *val;
                CONF_VALUE           *nval;
                void                 *ext_str = NULL;
                const X509V3_EXT_METHOD    *meth = X509V3_EXT_get(ext);
                if (!meth)
                    break;
                data = ext->value->data;

#if (OPENSSL_VERSION_NUMBER > 0x00907000L)
                if (meth->it)
                    ext_str = ASN1_item_d2i(NULL, (const unsigned char **)&data, ext->value->length,
                                            ASN1_ITEM_ptr(meth->it));
                else
                    ext_str = meth->d2i(NULL, (const unsigned char **)&data, ext->value->length);
#else
                ext_str = meth->d2i(NULL, &data, ext->value->length);
#endif
                val = meth->i2v(meth, ext_str, NULL);
                for (j = 0;  j < sk_CONF_VALUE_num(val);  j++)
                {
                    nval = sk_CONF_VALUE_value(val, j);
                    if (!strcmp(nval->name, "DNS") && !strcmp(nval->value, host))
                    {
                        ok = 1;
                        break;
                    }
                }
            }
            if (ok)
                break;
        }
    }
 
    if (!ok && (subj = X509_get_subject_name(cert)) &&
        X509_NAME_get_text_by_NID(subj, NID_commonName, data, 256) > 0)
    {
        data[255] = 0;
        if (strcasecmp(data, host) != 0)
            goto err_occured;
    }
 
    X509_free(cert);
    return SSL_get_verify_result(ssl);
 
err_occured:
    if (cert)
        X509_free(cert);
    return X509_V_ERR_APPLICATION_VERIFICATION;
}

FREEQ_EXPORT void seed_prng(void)
{
    RAND_load_file("/dev/urandom", 1024);
}

void showcerts(SSL* ssl)
{   X509 *cert;
    char *line;

    cert = SSL_get_peer_certificate(ssl);	/* Get certificates (if available) */
    if ( cert != NULL )
    {
        printf("Server certificates:\n");
        line = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
        printf("Subject: %s\n", line);
        free(line);
        line = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);
        printf("Issuer: %s\n", line);
        free(line);
        X509_free(cert);
    }
    else
        printf("No certificates.\n");
}

