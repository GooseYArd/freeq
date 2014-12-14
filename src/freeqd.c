#include <config.h>
#include "system.h"
#include "progname.h"

#include <assert.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

#include <signal.h>
#include <stdbool.h>

#include "sqlite4.h"
#include "freeq/libfreeq.h"
#include "libfreeq-private.h"

#include <arpa/inet.h>

/* control */
#include "control/stralloc.h"
#include "control/constmap.h"
#include "control/control.h"
#include "control/qsutil.h"

#define SERVER_PORT 13001
#define SQL_PORT 13000
#define ERROR 1
#define SUCCESS 0
#define MAX_MSG 8192

#define int_error(msg) handle_error(__FILE__, __LINE__, msg)
#include "ssl-common.h"

int server;
int recvstop = 0;
pthread_mutex_t mtx_shutdown = PTHREAD_MUTEX_INITIALIZER;
pthread_t t_sighandler;

typedef struct {
	int             num_active;
	pthread_cond_t  thread_exit_cv;
	pthread_mutex_t mutex;
	int             received_shutdown_req; /* 0=false, 1=true */
} thread_info_t;

thread_info_t pthread_info;

sqlite4 *pDb;

struct freeqd_state {
	freeq_generation_t **current;
	GRWLock rw_lock;
};

struct conn_ctx {
	sqlite4 *pDb;
	SSL *ssl;
	struct freeq_ctx *freeqctx;
	struct freeqd_state *fst;
};

struct CRYPTO_dynlock_value
{
    pthread_mutex_t mutex;
};

static pthread_mutex_t *mutex_buf = NULL;
DH *dh512 = NULL;
DH *dh1024 = NULL;

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

SSL_CTX *setup_server_ctx(void)
{
    SSL_CTX *ctx;

    ctx = SSL_CTX_new(SSLv23_method());
    if (SSL_CTX_load_verify_locations(ctx, CAFILE, CADIR) != 1)
	int_error("Error loading CA file and/or directory");
    if (SSL_CTX_set_default_verify_paths(ctx) != 1)
	int_error("Error loading default CA file and/or directory");
    if (SSL_CTX_use_certificate_chain_file(ctx, CERTFILE) != 1)
	int_error("Error loading certificate from file");

    if (SSL_CTX_use_PrivateKey_file(ctx, CERTFILE, SSL_FILETYPE_PEM) != 1)
	int_error("Error loading private key from file");

    SSL_CTX_set_verify(ctx,
		       SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
		       verify_callback);
    SSL_CTX_set_verify_depth(ctx, 4);
    SSL_CTX_set_options(ctx, SSL_OP_ALL | SSL_OP_NO_SSLv2 |
			SSL_OP_SINGLE_DH_USE);
    SSL_CTX_set_tmp_dh_callback(ctx, tmp_dh_callback);
    if (SSL_CTX_set_cipher_list(ctx, CIPHER_LIST) != 1)
	int_error("Error setting cipher list (no valid ciphers)");

    return ctx;
}

void handle_table(struct freeq_ctx *ctx, SSL *ssl)
{
	BIO  *buf_io, *ssl_bio;
	//char rbuf[1024];
	//char wbuf[1024];

	buf_io = BIO_new(BIO_f_buffer());         /* create a buffer BIO */
	ssl_bio = BIO_new(BIO_f_ssl());           /* create an ssl BIO */
	BIO_set_ssl(ssl_bio, ssl, BIO_CLOSE);     /* assign the ssl BIO to SSL */
	BIO_push(buf_io, ssl_bio);                /* add ssl_bio to buf_io */

	/* ret = BIO_puts(buf_io, wbuf); */
	/* Write contents of wbuf[] into buf_io */
	/* ret = BIO_write(buf_io, wbuf, wlen); */
	/* Write wlen-byte contents of wbuf[] into buf_io */

	/* ret = BIO_gets(buf_io, rbuf, READBUF_SIZE);  */
	/* Read data from buf_io and store in rbuf[] */
	/* ret = BIO_read(buf_io, rbuf, rlen);            */
	/* Read rlen-byte data from buf_io and store rbuf[] */
}

void g_hash_destroy_freeq_table(gpointer data) {
	struct freeq_table *tbl = (struct freeq_table *)data;
	freeq_table_unref(tbl);
}

int generation_table_merge(struct freeq_ctx *ctx, struct freeqd_state *fst, BIO *bio)
{
	struct freeq_table *tbl;
	struct freeq_table *curtbl;
	freeq_generation_t *gen = *(fst->current);
	int err;

	/* this needs to be moved outside this function */
	err = freeq_table_bio_read(ctx, &tbl, bio, gen->strings);
	if (err)
	{
		dbg(ctx, "unable to read tabledata\n");
		return err;
	}

	/* if we need to be able to replace node/tables in a
	 * generation, another option would be to use a tree instead
	 * of a linked list. Or we could store a pointer to the head
	 * and tail of each senders segment of the list and remove
	 * it */

	g_rw_lock_writer_lock(&(gen->rw_lock));
	curtbl = (struct freeq_table *)g_hash_table_lookup(gen->tables, tbl->name);
	if (curtbl != NULL)
	{
		dbg(ctx, "at least one host has sent %s for this generation\n", tbl->name);
		if (curtbl->senders != NULL && g_hash_table_contains(curtbl->senders, tbl->identity))
		{
			/* we already put a table from this sender into this generation
			   what should we do?
			 */
			dbg(ctx, "uh oh, we heard from this sender twice in a generation :(\n");
		}
		else
		{
			/* merge the tables */
			dbg(ctx, "would be merging tables here!\n");
		}
		g_rw_lock_writer_unlock(&(gen->rw_lock));
		return 1;
	}

	g_hash_table_insert(gen->tables, tbl->name, tbl);
	g_rw_lock_writer_unlock(&(gen->rw_lock));

}

/* int freeq_table_add_sender() */
/* { */
/*	/\* we have not received any instances of this table yet *\/ */
/*	GHashTable* hash = g_hash_table_new_full(g_str_hash, */
/*						 g_str_equal, */
/*						 g_free, */
/*						 NULL); */
/* //						 (GDestroyNotify)destroy_sender_table); */
/*	if (hash == NULL) { */
/*		g_rw_lock_writer_unlock(gen->rw_lock); */
/*		err(ctx, "free a bunch of stuff and return\n"); */
/*		//freeq_table_unref(tbl); */
/*		return 1; */
/*	} */

/*	gen->tables = hash; */
/*	g_hash_table_insert(hash, tbl->name, tbl); */
/*	g_hash_table_insert(gen->tables, tbl->name, hash); */
/* } */

int bio_peername(BIO *bio, char **hostname)
{
	int sock_fd;
	if (BIO_get_fd(bio, &sock_fd) == -1)
	{
		fprintf(stderr, "Uninitialized socket passed to worker");
		return 1;
	}

	printf("socket fd: %i\n", sock_fd);
	struct sockaddr addr;
	socklen_t addr_len;

	char *hn = malloc(sizeof(char) * 80);
	if (!hostname)
	{
		fprintf(stderr, "Out of memory\n");
		return 1;
	}

	addr_len = sizeof(addr);
	getpeername(sock_fd, &addr, &addr_len);
	if (addr.sa_family == AF_INET)
		inet_ntop(AF_INET,
			  &((struct sockaddr_in *)&addr)->sin_addr,
			  hn, 40);
	else if (addr.sa_family == AF_INET6)
		inet_ntop(AF_INET6,
			  &((struct sockaddr_in6 *)&addr)->sin6_addr,
			  hn, 40);
	else
	{
		fprintf(stderr, "Unknown socket type passed to worker(): %i\n",
			addr.sa_family);
	}

	*hostname = hn;
	return 0;
}

void* conn_handler(void *arg)
{
	long err;
	struct conn_ctx *ctx = (struct conn_ctx *)arg;
	SSL *ssl = ctx->ssl;
	pthread_detach(pthread_self());

	if (SSL_accept(ssl) <= 0)
	{
		int_error("Error accepting SSL connection");
		return NULL;
	}

	if ((err = post_connection_check(ssl, "localhost")) != X509_V_OK)
	{
		fprintf(stderr, "-Error: peer certificate: %s\n",
			X509_verify_cert_error_string(err));
		int_error("Error checking SSL object after connection");
	}

	BIO  *buf_io, *ssl_bio;
	buf_io = BIO_new(BIO_f_buffer());
	ssl_bio = BIO_new(BIO_f_ssl());
	BIO_set_ssl(ssl_bio, ssl, BIO_CLOSE);
	BIO_push(buf_io, ssl_bio);

	fprintf(stderr, "SSL Connection opened\n");
	if (generation_table_merge(ctx->freeqctx, ctx->fst, buf_io))
	{
		fprintf(stderr, "table merge failed\n");
		SSL_shutdown(ssl);
	}
	else
	{
		fprintf(stderr, "table merged ok\n");
		SSL_clear(ssl);
	}

	fprintf(stderr, "SSL Connection closed\n");
	SSL_free(ssl);

	ERR_remove_state(0);

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

int conn_cleanup(void)
{
    int i;
    if (!mutex_buf)
	return 0;
    CRYPTO_set_id_callback(NULL);
    CRYPTO_set_locking_callback(NULL);
    for (i = 0; i < CRYPTO_num_locks(); i++)
	pthread_mutex_destroy(&(mutex_buf[i]));
    free(mutex_buf);
    mutex_buf = NULL;
    return 1;
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

const char *freeq_sqlite_typexpr[] = {
	"NULL",
	"VARCHAR(255)",
	"INTEGER",
	"INTEGER",
	"INTEGER",
	"INTEGER"
};

const freeq_coltype_t sqlite_to_freeq_coltype[] = {
	FREEQ_COL_NULL,   // 0 undefined
	FREEQ_COL_NUMBER, // 1 SQLITE_INTEGER,
	FREEQ_COL_NUMBER, // 2 SQLITE_FLOAT,
	FREEQ_COL_STRING, // 3 SQLITE_TEXT,
	FREEQ_COL_STRING, // 4 SQLITE_BLOB,
	FREEQ_COL_NULL,   // 5 SQLITE_NULL
};

int table_ddl(struct freeq_ctx *ctx, struct freeq_table *tbl, GString *stm)
{
	if (stm == NULL)
		return 1;

	g_string_printf(stm, "CREATE TABLE IF NOT EXISTS %s (", tbl->name);

	for (int i = 0; i < tbl->numcols; i++)
	{
		dbg(ctx, "create_table: type is %d\n", tbl->columns[i].coltype);
		g_string_append_printf(stm,
				       "%s %s%s",
				       tbl->columns[i].name,
				       freeq_sqlite_typexpr[tbl->columns[i].coltype],
				       i < (tbl->numcols - 1) ? "," : "");
	}
	//stm << ");";
	stm = g_string_append(stm, ");");
	dbg(ctx, "%s\n", stm);
	return 0;
}

int ddl_insert(struct freeq_ctx *ctx, struct freeq_table *tbl, GString *stm)
{
	g_string_printf(stm, "INSERT INTO %s VALUES (", tbl->name);
	for (int i = 1; i < tbl->numcols; i++)
		g_string_append_printf(stm, "?%d,", i);
	g_string_append_printf(stm, "?%d);", tbl->numcols);
	dbg(ctx, "statement: %s\n", stm->str);
}

int gen_to_db(struct freeq_ctx *ctx, freeq_generation_t *g, sqlite4 *mDb)
{
	GHashTableIter iter;
	guint size = g_hash_table_size(g->tables);

	uint32_t *val;
	uint32_t *key_;

	g_hash_table_iter_init(&iter, g->tables);
	while (g_hash_table_iter_next(&iter, (gpointer) &key_, (gpointer) &val))
	{
		printf("key %u ---> %u\n", (uint32_t)*key_, (uint32_t)*val);
	}

	return 0;

}

int tb_to_db(struct freeq_ctx *ctx, struct freeq_table *tbl, sqlite4 *mDb)
{
	sqlite4_stmt *stmt;
	GString *sql = g_string_sized_new(255);

	GSList *colp[tbl->numcols];
	memset(colp, 0, tbl->numcols * sizeof(GSList *) );
	for (int j = 0; j < tbl->numcols; j++)
		colp[j] = tbl->columns[j].data;

	const char **strarrp = NULL;
	int *intarrp = NULL;
	int res;

	if (sqlite4_exec(mDb, "BEGIN TRANSACTION;", NULL, NULL) != SQLITE4_OK)
	{
		dbg(ctx, "unable to start transaction\n");
		return 1;
	}

	g_string_printf(sql, "DROP TABLE %s;", tbl->name);
	if (sqlite4_exec(mDb, sql->str, NULL, NULL) != SQLITE4_OK)
		dbg(ctx, "failed to drop table, ignoring");

	table_ddl(ctx, tbl, sql);
	if (sqlite4_exec(mDb, sql->str, NULL, NULL) != SQLITE4_OK)
	{
		dbg(ctx, "failed to create table, rolling back");
		sqlite4_exec(mDb, "ROLLBACK;", NULL, NULL);
		g_string_free(sql, 1);
		return 1;
	}

	ddl_insert(ctx, tbl, sql);
	if (sqlite4_prepare(mDb, sql->str, sql->len, &stmt, NULL) != SQLITE4_OK)
	{
		dbg(ctx, "failed to create statement (%d), rolling back", res);
		sqlite4_exec(mDb, "ROLLBACK;", NULL, NULL);
		g_string_free(sql,1);
		return 1;
	}

	g_string_free(sql,1);
	for (unsigned i = 0; i < tbl->numrows; i++)
	{
		for (int j = 0; j < tbl->numcols; j++)
		{
			switch (tbl->columns[i].coltype)
			{
			case FREEQ_COL_STRING:
				sqlite4_bind_text(stmt, j, colp[j]->data, strlen(colp[j]->data), SQLITE4_TRANSIENT, NULL);
				if (res != SQLITE4_OK)
				{
					dbg(ctx, "failed bind: %s", sqlite4_errmsg(mDb));
				}
				break;
			case FREEQ_COL_NUMBER:
				res = sqlite4_bind_int(stmt, j, GPOINTER_TO_INT(colp[j]->data));
				if (res != SQLITE4_OK)
				{
					dbg(ctx, "failed bind: %s", sqlite4_errmsg(mDb));
				}
				break;
			default:
				break;
			}
			colp[j] = g_slist_next(colp[j]);
		}
		if (sqlite4_step(stmt) != SQLITE4_DONE)
		{
			dbg(ctx, "execute failed: %s\n", sqlite4_errmsg(mDb));
			sqlite4_exec(mDb, "ROLLBACK;", NULL, NULL);
			sqlite4_finalize(stmt);
			return -1;
		} else {
			sqlite4_reset(stmt);
		}
	}

	dbg(ctx, "committing transaction\n");
	res = sqlite4_exec(mDb, "COMMIT TRANSACTION;", NULL, NULL);
	dbg(ctx, "result of commit was %d\n", res);
	sqlite4_finalize(stmt);
}

void *status_logger (void *arg)
{
	struct conn_ctx *ctx = (struct conn_ctx *)arg;
	freeq_generation_t *curgen;

	printf("STATUS_LOGGER STARTING\n");
	time_t dt;
	while (1)
	{
		sleep(5);
		//dbg(ctx->freeqctx, "mark\n");
		curgen = *(ctx->fst->current);
		dt = time(NULL) - curgen->era;
		if (dt > 30)
		{
			freeq_generation_t *newgen;
			dbg(ctx->freeqctx, "generation is ready to publish\n");
			if (freeq_generation_new(&newgen))
			{
				dbg(ctx->freeqctx, "unable to allocate generation\n");
				continue;
			}

			/* advance generation, take pointer to previous generation */
			g_rw_lock_writer_lock(&(ctx->fst->rw_lock));
			ctx->fst->current = &newgen;
			g_rw_lock_writer_unlock(&(ctx->fst->rw_lock));

			g_rw_lock_writer_lock(&(curgen->rw_lock));

			g_rw_lock_writer_unlock(&(curgen->rw_lock));

			/* to_db on previous generation */
			/* free previous generation */


		} else {
			dbg(ctx->freeqctx, "generation age is %d\n", dt);
		}
	}
}

void *monitor (void *arg)
{
	sigset_t catchsig;
	int sigrecv;

	sigemptyset(&catchsig);
	sigaddset(&catchsig, SIGINT);
	sigaddset(&catchsig, SIGSTOP);

	sigwait(&catchsig, &sigrecv);
	pthread_mutex_lock(&pthread_info.mutex);
	pthread_info.received_shutdown_req = 1;
	while (pthread_info.num_active > 0)
		pthread_cond_wait(&pthread_info.thread_exit_cv, &pthread_info.mutex);

	pthread_mutex_unlock(&pthread_info.mutex);
	exit(0);
	return(NULL);

	// struct receiver_info *ri = (struct receiver_info *)arg;
	// freeq_ctx *ctx;
	// err = freeq_new(&ctx, "appname", "identity");
	// freeq_set_log_priority(ctx, 10);

}


void cleanup(int server)
{
	fprintf(stderr, "received SIGINT, stopping server...");
	//close(server);
	pthread_exit(NULL);
	fprintf(stderr, " bye!\n");
	return;
}

/**
 * readline() - read an entire line from a file descriptor until a newline.
 * functions returns the number of characters read but not including the
 * null character.
 **/
int readline(int fd, char *str, int maxlen)
{
	int n;
	int readcount;
	char c;
	for (n = 1; n < maxlen; n++)
	{
		readcount = read(fd, &c, 1);
		if (readcount == 1)
		{
			*str = c;
			str++;
			if (c == '\n')
				break;
		}
		else if (readcount == 0)
		{
			if (n == 1)
				return (0);
			else
				break;
		}
		else
			return (-1);
	}
	*str=0;
	return (n);
}

int readnf (int fd, char *line)
{
	if (readline(fd, line, MAX_MSG) <= 0)
		return ERROR;
	return SUCCESS;
}

void* sqlhandler(void *paramsd) {
	struct freeq_ctx *ctx;
	struct sockaddr_in cliAddr;
	char line[MAX_MSG];
	int res;
	int client_local;

	int bsize;
	const char *s;
	char **strings;
	int *vals;

	struct freeq_table *tblp;
	struct freeq_column *colp;
	struct freeq_column_segment *seg;

	sqlite4_stmt *pStmt;
	socklen_t addr_len;
	int segment_size = 2000;

	BIO *out;
	//BIO_free(out);

	client_local = *((int *)paramsd);
	addr_len = sizeof(cliAddr);

	getpeername(client_local, (struct sockaddr*)&cliAddr, &addr_len);
	memset(line, 0, MAX_MSG);

	res = freeq_new(&ctx, "freeqd_handler", "identity");
	freeq_set_log_priority(ctx, 10);
	if (res)
	{
		dbg(ctx, "unable to create freeq context\n");
		return NULL;
	}

	out = BIO_new_fd(client_local, BIO_NOCLOSE);

	while(!recvstop)
	{
		res = readline(client_local, line, MAX_MSG);
		dbg(ctx, "received query from client: \"%s\" res=%d\n", line, res);
		if (res == 0)
			break;

		if (res == 2)
			continue;

		res = sqlite4_prepare(pDb, line, strlen(line), &pStmt, 0);
		if (res != SQLITE4_OK)
		{
			dbg(ctx, "prepare failed, sending error, res was %d\n", res);
			//freeq_error_write_sock(ctx, sqlite4_errmsg(pDb), client_local);
			dbg(ctx, sqlite4_errmsg(pDb));
			BIO_printf(out, "error: %s\n", sqlite4_errmsg(pDb));
			sqlite4_finalize(pStmt);
			continue;
		}
		BIO_write(out, "statement prepared\n", 20);
		/* column type is unset until the query has been
		 * stepped once */
		if (sqlite4_step(pStmt) != SQLITE4_ROW)
		{
			//freeq_error_write_sock(ctx, sqlite4_errmsg(pDb), client_local);
			sqlite4_finalize(pStmt);
			continue;
		}

		if (!tblp)
		{
			dbg(ctx, "unable to allocate table\n");
			//freeq_error_write_sock(ctx, "ENOMEM", client_local);
			sqlite4_finalize(pStmt);
			continue;
		}

		int numcols = sqlite4_column_count(pStmt);
		dbg(ctx, "creating response table %s, %d columns\n", tblp->name, numcols);
		int j = 0;

		for (int i = 0; i < numcols; i++)
		{
			sqlite4_column_name(pStmt, i);
			freeq_coltype_t ctype = sqlite_to_freeq_coltype[sqlite4_column_type(pStmt, i)];
		}

		do {
			BIO_write(out, "row\n", 4);
		} while (sqlite4_step(pStmt) == SQLITE4_ROW);

		//dbg(ctx, "setting table, length %d\n", tblp->numrows);
		sqlite4_finalize(pStmt);
		dbg(ctx, "sending result");
		//freeq_table_write_sock(ctx, tblp, client_local);
		memset(line, 0, MAX_MSG);
		freeq_table_unref(tblp);
		sleep(1);
	}

	//freeq_unref(ctx);
	close(client_local);
 }


void *sqlserver(void *arg) {

	int client;
	int optval;
	int err;
	socklen_t addr_len;
	pthread_t thread;

	struct sockaddr_in cliAddr;
	struct sockaddr_in servAddr;
	struct freeq_ctx *ctx;
	err = freeq_new(&ctx, "appname", "identity");
	if (err < 0)
		exit(EXIT_FAILURE);

	freeq_set_log_priority(ctx, 10);

	signal(SIGINT, cleanup);
	signal(SIGTERM, cleanup);
	signal(SIGPIPE, SIG_IGN);

	server = socket(PF_INET, SOCK_STREAM, 0);
	if (server < 0)
	{
		dbg(ctx, "cannot open socket ");
		freeq_unref(ctx);
		return NULL;
	}

	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servAddr.sin_port = htons(SQL_PORT);
	memset(servAddr.sin_zero, 0, 8);
	optval = 1;

	setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);

	if (bind(server, (struct sockaddr *) &servAddr, sizeof(struct sockaddr)) < 0)
	{
		dbg(ctx, "cannot bind port ");
		freeq_unref(ctx);
		return NULL;
	}

	listen(server, 5);

	while (!recvstop)
	{
		dbg(ctx, "waiting for data on port %u\n", SQL_PORT);
		addr_len = sizeof(cliAddr);
		client = accept(server, (struct sockaddr *) &cliAddr, &addr_len);
		if (client < 0)
		{
			dbg(ctx, "cannot accept connection ");
			break;
		}
		dbg(ctx, "accepted connection, creating sqlhandler thread\n");
		pthread_create(&thread, 0, &sqlhandler, &client);
	}
	close(server);
	freeq_unref(ctx);
	exit(0);
}

int init_freeqd_state(struct freeq_ctx *freeqctx, struct freeqd_state *s)
{
	freeq_generation_t *fgen;
	if (freeq_generation_new(&fgen))
	{
		dbg(freeqctx, "unable to allocate generation\n");
		return 1;
		//exit(EXIT_FAILURE);
	}
	g_rw_lock_init(&(s->rw_lock));
	s->current = &fgen;
}

int
main (int argc, char *argv[])
{
	int err;
	int res;
	sigset_t set;

	BIO *acc, *client;
	SSL *ssl;
	SSL_CTX *sslctx;
	pthread_t tid;

	struct freeqd_state fst;

	//pthread_t t_receiver;
	//pthread_t t_aggregator;
	//pthread_t t_recvinvite;
	pthread_t t_monitor;
	pthread_t t_status_logger;
	pthread_t t_sqlserver;

	static stralloc clients = {0};
	static stralloc aggport = {0};
	//stralloc cliport = {0};

	set_program_name(argv[0]);
	setlocale(LC_ALL, "");

#if ENABLE_NLS
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
#endif

	struct freeq_ctx *freeqctx;
	err = freeq_new(&freeqctx, "appname", "identity");
	if (err < 0)
		exit(EXIT_FAILURE);

	freeq_set_log_priority(freeqctx, 10);

	sigemptyset(&set);
	sigaddset(&set, SIGHUP);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGUSR1);
	sigaddset(&set, SIGUSR2);
	sigaddset(&set, SIGALRM);
	pthread_sigmask(SIG_BLOCK, &set, NULL);

	dbg(freeqctx, "starting monitor thread\n");
	pthread_create(&t_monitor, 0, &monitor, pDb);

	init_freeqd_state(freeqctx, &fst);

	struct conn_ctx status_ctx;
	status_ctx.ssl = ssl;
	status_ctx.pDb = pDb;
	status_ctx.fst = &fst;
	status_ctx.freeqctx = freeqctx;
	struct conn_ctx *sctx = &status_ctx;
	pthread_create(&t_status_logger, 0, &status_logger, sctx);

	dbg(freeqctx, "reading clients file\n");
	if (control_readfile(&clients,(char *)"control/clients",1))
	{
		dbg(freeqctx, "clients present, setting up agg database\n");
		//pthread_create(&t_aggregator, 0, &aggregator, (void *)&ri);
		//res = sqlite4_open(0, ":memory:", &pDb, 0);
		res = sqlite4_open(0, "ondisk.db", &pDb, SQLITE4_OPEN_READWRITE | SQLITE4_OPEN_CREATE,NULL);
		if (res != SQLITE4_OK)
		{
			fprintf(stderr, "failed to open in-memory db\n");
			exit(res);
		}
	} else {
		dbg(freeqctx, "failed to read clients file, database not initialized\n");
	}
	//if (control_readfile(&clients,"",1) != 1)
	//	pthread_create(&t_receiver, 0, &receiver, (void *)&ri);
	//	pthread_create(&t_recvinvite, 0, &recvinvite, (void *)recvinvite_nnuri);
	//}

	struct conn_ctx sqlctx;
	sqlctx.pDb = pDb;
	pthread_create(&t_sqlserver, 0, &sqlserver, (void *)&sqlctx);

//	res = sqlite4_exec(pDb, "create table if not exists freeq_stats(int last);", log_monitor, NULL);
//	if (res != SQLITE4_OK)
//	{
//		printf("creating freeq_stats failed: %d %s\n", res, sqlite4_errmsg(pDb));
//	}

	dbg(freeqctx, "initializing SSL mutexes\n");
	mutex_buf = (pthread_mutex_t *)malloc(CRYPTO_num_locks() *sizeof(pthread_mutex_t));
	if (!mutex_buf)
	{
		dbg(freeqctx, "unable to allocate mutex buf, bailing\n");
		exit(1);
	}

	for (int i = 0; i < CRYPTO_num_locks(); i++)
		pthread_mutex_init(&(mutex_buf[i]), NULL) ;

	dbg(freeqctx, "setting static lock callbacks\n");
	CRYPTO_set_id_callback(id_function);
	CRYPTO_set_locking_callback(locking_function);
	dbg(freeqctx, "setting dynamic lock callbacks\n");
	CRYPTO_set_dynlock_create_callback(dyn_create_function);
	CRYPTO_set_dynlock_lock_callback(dyn_lock_function);
	CRYPTO_set_dynlock_destroy_callback(dyn_destroy_function);

	dbg(freeqctx, "seeding PRNG\n");
	seed_prng();
	dbg(freeqctx, "initializing ssl library\n");
	SSL_library_init();
	SSL_load_error_strings();

	sslctx = setup_server_ctx();

	dbg(freeqctx, "setting static lock callbacks\n");
	res = control_readline(&aggport, "control/aggport");
	if (!res)
		stralloc_cats(&aggport, "localhost:13001");
	stralloc_0(&aggport);

	dbg(freeqctx, "starting aggregation listener on %s\n", aggport);
	acc = BIO_new_accept("127.0.0.1:13001");
	if (!acc)
		int_error("Error creating server socket");

	if (BIO_do_accept(acc) <= 0)
		int_error("Error binding server socket");

	for (;;)
	{
		dbg(freeqctx, "waiting for connection\n");
		if (BIO_do_accept(acc) <= 0)
			int_error("Error accepting connection");
		dbg(freeqctx, "accepted connection, setting up ssl\n");
		/*bio_peername(acc);*/

		client = BIO_pop(acc);
		if (!(ssl = SSL_new(sslctx)))
			int_error("Error creating SSL context");
		dbg(freeqctx, "session creation, setting bio\n");
		SSL_set_bio(ssl, client, client);

		dbg(freeqctx, "spawning thread\n");
		struct conn_ctx *ctx = malloc(sizeof(struct conn_ctx));
		ctx->ssl = ssl;
		ctx->pDb = pDb;
		ctx->fst = &fst;
		ctx->freeqctx = freeqctx;

		pthread_create(&tid, 0, &conn_handler, ctx);
		// TODO: free ctx

	}

	SSL_CTX_free(sslctx);
	BIO_free(acc);
	freeq_unref(freeqctx);

}
