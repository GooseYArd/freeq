#include <config.h>
#include "system.h"
#include "progname.h"

#include <assert.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

#include <signal.h>
#include <stdbool.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "sqlite4.h"
#include "freeq/libfreeq.h"
#include "libfreeq-private.h"

#include <arpa/inet.h>

/* control */
#include "control/stralloc.h"
#include "control/constmap.h"
#include "control/control.h"
#include "control/qsutil.h"

#define MAX_MSG 8192
#include "ssl-common.h"

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
        freeq_generation_t *current;
        GRWLock rw_lock;
};

struct srv_ctx {
        sqlite4 *pDb;
        struct freeq_ctx *freeqctx;
        struct freeqd_state *fst;
};

struct conn_ctx {
        struct srv_ctx *srvctx;
        BIO *client;
};

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
        freeq_generation_t *gen = fst->current;
        int err;

        /* this needs to be moved outside this function */
        err = freeq_table_bio_read(ctx, &tbl, bio, gen->strings);
        if (err)
        {
                dbg(ctx, "unable to read tabledata\n");
                return err;
        }

        freeq_table_print(ctx, tbl, stdout);

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
        return FREEQ_OK;
}

/* int freeq_table_add_sender() */
/* { */
/*	/\* we have not received any instances of this table yet *\/ */
/*	GHashTable* hash = g_hash_table_new_full(g_str_hash, */
/*                                               g_str_equal, */
/*                                               g_free, */
/*                                               NULL); */
/* //                                            (GDestroyNotify)destroy_sender_table); */
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
        SSL *ssl;
        struct conn_ctx *ctx = (struct conn_ctx *)arg;
        struct freeq_ctx *freeqctx = ctx->srvctx->freeqctx;
        struct freeqd_state *fst = ctx->srvctx->fst;
        BIO *client = ctx->client;

        pthread_detach(pthread_self());

        ssl = freeq_ssl_new(freeqctx);
        SSL_set_bio(ssl, client, client);
        if (SSL_accept(ssl) <= 0)
        {
                int_error("Error accepting SSL connection");
                return NULL;
        }

        if ((err = post_connection_check(freeqctx, ssl, "localhost")) != X509_V_OK)
        {
                err(freeqctx, "error: peer certificate: %s\n", X509_verify_cert_error_string(err));
                int_error("Error checking SSL object after connection");
        }

        BIO  *buf_io, *ssl_bio;
        buf_io = BIO_new(BIO_f_buffer());
        ssl_bio = BIO_new(BIO_f_ssl());
        BIO_set_ssl(ssl_bio, ssl, BIO_CLOSE);
        BIO_push(buf_io, ssl_bio);

        dbg(freeqctx, "ssl client connection opened\n");
        if (generation_table_merge(freeqctx, fst, buf_io))
        {
                err(freeqctx, "table merge failed\n");
                SSL_shutdown(ssl);
        }
        else
        {
                dbg(freeqctx, "table merged ok\n");
                SSL_clear(ssl);
        }

        dbg(freeqctx, "ssl client connection closed\n");
        SSL_free(ssl);

        ERR_remove_state(0);
        return 0;
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

        g_string_append(stm, ");");
        dbg(ctx, "table_ddl sql: %s\n", stm->str);
        return 0;
}

int ddl_insert(struct freeq_ctx *ctx, struct freeq_table *tbl, GString *stm)
{
        g_string_printf(stm, "INSERT INTO %s VALUES (", tbl->name);
        for (int i = 1; i < tbl->numcols; i++)
                g_string_append_printf(stm, "?%d,", i);
        g_string_append_printf(stm, "?%d);", tbl->numcols);
        dbg(ctx, "statement: %s\n", stm->str);
        return 0;
}

int tbl_to_db(struct freeq_ctx *ctx, struct freeq_table *tbl, sqlite4 *mDb)
{
        sqlite4_stmt *stmt;
        GString *sql = g_string_sized_new(255);
        GSList *colp[tbl->numcols];

        memset(colp, 0, tbl->numcols * sizeof(GSList *) );
        for (int j = 0; j < tbl->numcols; j++)
                colp[j] = tbl->columns[j].data;

        int res;

        freeq_table_print(ctx, tbl, stdout);

        if (sqlite4_exec(mDb, "BEGIN TRANSACTION;", NULL, NULL) != SQLITE4_OK)
        {
                dbg(ctx, "unable to start transaction: %s\n", sqlite4_errmsg(mDb));
                return 1;
        }

        g_string_printf(sql, "DROP TABLE %s;", tbl->name);
        if (sqlite4_exec(mDb, sql->str, NULL, NULL) != SQLITE4_OK)
                dbg(ctx, "failed to drop table, ignoring\n");

        table_ddl(ctx, tbl, sql);
        if (sqlite4_exec(mDb, sql->str, NULL, NULL) != SQLITE4_OK)
        {
                dbg(ctx, "failed to create table, rolling back\n");
                sqlite4_exec(mDb, "ROLLBACK;", NULL, NULL);
                g_string_free(sql, 1);
                return 1;
        }

        ddl_insert(ctx, tbl, sql);
        if ((res = sqlite4_prepare(mDb, sql->str, sql->len, &stmt, NULL)) != SQLITE4_OK)
        {
                dbg(ctx, "failed to create statement (%d), rolling back\n", res);
                sqlite4_exec(mDb, "ROLLBACK;", NULL, NULL);
                g_string_free(sql,1);
                return 1;
        }

        g_string_free(sql,1);
        for (uint32_t i = 0; i < tbl->numrows; i++)
        {
                for (uint32_t j = 0; j < tbl->numcols; j++)
                {
                        switch (tbl->columns[j].coltype)
                        {
                        case FREEQ_COL_STRING:
                                res = sqlite4_bind_text(stmt,
                                                        j+1,
                                                        colp[j]->data == NULL ? "" : colp[j]->data,
                                                        colp[j]->data == NULL ? 0 : strlen(colp[j]->data),
                                                        SQLITE4_TRANSIENT, NULL);
                                if (res != SQLITE4_OK)
                                {
                                        dbg(ctx, "stmt: %s\n", (char *)stmt);
                                        dbg(ctx, "row %d failed binding string column %d %s: %s (%d)\n", i, j, (char *)colp[j]->data, sqlite4_errmsg(mDb), res);
                                }
                                break;
                        case FREEQ_COL_NUMBER:
                                res = sqlite4_bind_int(stmt, j, GPOINTER_TO_INT(colp[j]->data));
                                if (res != SQLITE4_OK)
                                {
                                        dbg(ctx, "row %d failed bind: %s\n", i, sqlite4_errmsg(mDb));
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
        return 0;
}

int gen_to_db(struct freeq_ctx *ctx, freeq_generation_t *g, sqlite4 *mDb)
{
        GHashTableIter iter;
        gpointer key, val;
        struct freeq_table *t;

        g_hash_table_iter_init(&iter, g->tables);
        while (g_hash_table_iter_next(&iter, &key, &val))
        {
                dbg(ctx, "replacing table %s\n", (char *)key);
                t = (struct freeq_table *)val;
                if (tbl_to_db(ctx, t, mDb))
                {
                        err(ctx, "gen_to_db failed to publish %s", (char *)key);
                }
                else
                {
                        dbg(ctx, "published %s\n", (char *)key);
                }
        }
        return 0;
}

void *status_logger (void *arg)
{
        struct srv_ctx *srv = (struct srv_ctx *)arg;
        struct freeq_ctx *freeqctx = srv->freeqctx;
        struct freeqd_state *fst = srv->fst;
        freeq_generation_t *curgen;

        dbg(freeqctx, "status_logger starting\n");
        time_t dt;
        while (1)
        {
                sleep(5);
                curgen = fst->current;
                dt = time(NULL) - curgen->era;
                if (dt > 10)
                {
                        freeq_generation_t *newgen;
                        //dbg(freeqctx, "generation is ready to publish\n");
                        if (freeq_generation_new(&newgen))
                        {
                                dbg(freeqctx, "unable to allocate generation\n");
                                continue;
                        }

                        /* advance generation, take pointer to previous generation */
                        g_rw_lock_writer_lock(&(fst->rw_lock));
                        fst->current = newgen;
                        g_rw_lock_writer_unlock(&(fst->rw_lock));

                        /* to_db on previous generation */
                        g_rw_lock_writer_lock(&(curgen->rw_lock));
                        gen_to_db(freeqctx, curgen, srv->pDb);
                        g_rw_lock_writer_unlock(&(curgen->rw_lock));
                        /* free previous generation */

                } else {
                        //dbg(freeqctx, "generation age is %d\n", (int)dt);
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
        pthread_exit(NULL);
        fprintf(stderr, " bye!\n");
        return;
}

void* sqlhandler(void *arg) {
        char sql[MAX_MSG];
        int ret, err;
        SSL *ssl;
        sqlite4_stmt *pStmt;

        struct conn_ctx *conn = (struct conn_ctx *)arg;
        struct freeq_ctx *freeqctx = conn->srvctx->freeqctx;
        BIO *client = conn->client;

        if (!(ssl = freeq_ssl_new(freeqctx)))
        {
                err(freeqctx, "couldn't allocate new ssl instance");
                BIO_free_all(client);
                free(conn);
                pthread_exit(NULL);
        }

        SSL_set_bio(ssl, client, client);
        if (SSL_accept(ssl) <= 0)
        {
                int_error("Error accepting SSL connection");
                BIO_free_all(client);
                free(conn);
                pthread_exit(NULL);
        }

        if ((err = post_connection_check(freeqctx, ssl, "localhost")) != X509_V_OK)
        {
                err(freeqctx, "error: peer certificate: %s\n", X509_verify_cert_error_string(err));
                BIO_free_all(client);
                free(conn);
                pthread_exit(NULL);
        }

        BIO *b, *ssl_bio;
        b = BIO_new(BIO_f_buffer());
        ssl_bio = BIO_new(BIO_f_ssl());
        BIO_set_ssl(ssl_bio, ssl, BIO_CLOSE);
        BIO_push(b, ssl_bio);

        memset(sql, 0, MAX_MSG);
        ret = BIO_gets(b, sql, MAX_MSG);

        ret = sqlite4_prepare(pDb, sql, strlen(sql), &pStmt, 0);
        if (ret != SQLITE4_OK)
        {
                dbg(freeqctx, "prepare failed for %s sending error, ret was %d\n", sql, ret);
                //freeq_error_write_sock(freeqctx, sqlite4_errmsg(pDb), b);
                dbg(freeqctx, sqlite4_errmsg(pDb));
                //BIO_printf(b, "error: %s\n", sqlite4_errmsg(pDb));
                sqlite4_finalize(pStmt);
                //pthread_exit(FREEQ_ERR);
        }

        freeq_sqlite_to_bio(freeqctx, b, pStmt);

        SSL_shutdown(ssl);
        SSL_free(ssl);

        BIO_free_all(client);
        free(conn);
        pthread_exit(FREEQ_OK);

}

void *cliserver(void *arg) {

        struct srv_ctx *srv = (struct srv_ctx *)arg;
        struct freeq_ctx *freeqctx = srv->freeqctx;
        struct conn_ctx *conn_ctx;
        int res;
        pthread_t tid;
        BIO *acc, *client;

        static stralloc aggport = {0};

        res = control_readline(&aggport, "control/aggport");
        if (!res)
        {
                err(freeqctx, "unable to read control/aggport");
                exit(FREEQ_ERR);
        }

        stralloc_0(&aggport);

        dbg(freeqctx, "starting aggregation listener on %s\n", (char *)aggport.s);
        acc = BIO_new_accept((char *)aggport.s);
        if (!acc)
        {
                int_error("Error creating server socket");
                exit(FREEQ_ERR);
        }

        if (BIO_do_accept(acc) <= 0)
                int_error("Error binding server socket");

        for (;;)
        {
                dbg(freeqctx, "waiting for connection\n");
                if (BIO_do_accept(acc) <= 0)
                        int_error("Error accepting connection");
                dbg(freeqctx, "accepted connection, setting up ssl\n");

                //bio_peername(acc);

                client = BIO_pop(acc);
                conn_ctx = malloc(sizeof(struct conn_ctx));
                conn_ctx->srvctx = srv;
                conn_ctx->client = client;
                pthread_create(&tid, 0, &conn_handler, (void *)conn_ctx);
        }
}

void *sqlserver(void *arg) {
        struct srv_ctx *srv = (struct srv_ctx *)arg;
        struct freeq_ctx *freeqctx = srv->freeqctx;
        int res;
        pthread_t thread;
        BIO *acc, *client;

        signal(SIGINT, cleanup);
        signal(SIGTERM, cleanup);
        signal(SIGPIPE, SIG_IGN);

        static stralloc sqlport = {0};

        res = control_readline(&sqlport, "control/sqlport");
        if (!res)
        {
                err(freeqctx, "unable to read control/sqlport\n");
                exit(FREEQ_ERR);
        }

        stralloc_0(&sqlport);
        dbg(freeqctx, "starting query listener on %s\n", (char *)sqlport.s);
        acc = BIO_new_accept((char *)sqlport.s);
        if (!acc)
        {
                int_error("Error creating server socket");
                exit(FREEQ_ERR);
        }

        if (BIO_do_accept(acc) <= 0)
                int_error("Error binding server socket");

        for (;;)
        {
                if (BIO_do_accept(acc) <= 0)
                        int_error("Error accepting connection");
                client = BIO_pop(acc);
                struct conn_ctx *conn = malloc(sizeof(struct conn_ctx));
                conn->srvctx = srv;
                conn->client = client;
                pthread_create(&thread, 0, &sqlhandler, (void *)conn);
        }

        BIO_free(acc);
        pthread_exit(FREEQ_OK);;
}

int init_freeqd_state(struct freeq_ctx *freeqctx, struct freeqd_state *s)
{
        freeq_generation_t *fgen;
        if (freeq_generation_new(&fgen))
        {
                dbg(freeqctx, "unable to allocate generation\n");
                return FREEQ_ERR;
        }
        g_rw_lock_init(&(s->rw_lock));
        s->current = fgen;
        return 0;
}

int
main (int argc, char *argv[])
{
        int err;
        int res;
        sigset_t set;

        BIO *acc;
        SSL_CTX *sslctx;
        struct freeq_ctx *freeqctx;
        struct freeqd_state fst;

        pthread_t t_monitor;
        pthread_t t_status_logger;
        pthread_t t_sqlserver;
        pthread_t t_cliserver;
        static stralloc clients = {0};

        set_program_name(argv[0]);
        setlocale(LC_ALL, "");

#if ENABLE_NLS
        bindtextdomain(PACKAGE, LOCALEDIR);
        textdomain(PACKAGE);
#endif

        err = freeq_new(&freeqctx, "appname", "identity", FREEQ_SERVER);
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

        dbg(freeqctx, "reading clients file\n");
        if (control_readfile(&clients,(char *)"control/clients",1))
        {
                dbg(freeqctx, "clients present, setting up agg database\n");
                //pthread_create(&t_aggregator, 0, &aggregator, (void *)&ri);
                //res = sqlite4_open(0, ":memory:", &pDb, 0);
                res = sqlite4_open(0, "ondisk.db", &pDb, SQLITE4_OPEN_READWRITE | SQLITE4_OPEN_CREATE,NULL);
                if (res != SQLITE4_OK)
                {
                        err(freeqctx, "failed to open in-memory db\n");
                        exit(res);
                }
        } else {
                dbg(freeqctx, "failed to read clients file, database not initialized\n");
        }

        struct srv_ctx status_ctx;
        status_ctx.pDb = pDb;
        status_ctx.fst = &fst;
        status_ctx.freeqctx = freeqctx;
        struct srv_ctx *sctx = &status_ctx;

        pthread_create(&t_status_logger, 0, &status_logger, (void *)sctx);

        //if (control_readfile(&clients,"",1) != 1)
        //	pthread_create(&t_receiver, 0, &receiver, (void *)&ri);
        //	pthread_create(&t_recvinvite, 0, &recvinvite, (void *)recvinvite_nnuri);
        //}

        res = sqlite4_exec(pDb, "create table if not exists freeq_stats(int last);", NULL, NULL);
        if (res != SQLITE4_OK)
        {
                printf("creating freeq_stats failed: %d %s\n", res, sqlite4_errmsg(pDb));
        }

        pthread_create(&t_sqlserver, 0, &sqlserver, (void *)sctx);
        pthread_create(&t_cliserver, 0, &cliserver, (void *)sctx);

        while (1) {
                sleep(1);
        }

        SSL_CTX_free(sslctx);
        BIO_free(acc);
        freeq_unref(freeqctx);
        return 0;
}
