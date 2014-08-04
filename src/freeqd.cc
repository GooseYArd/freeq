#include <config.h>
#include "system.h"
#include "progname.h"

#include <assert.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <nanomsg/nn.h>
#include <nanomsg/pipeline.h>
#include <nanomsg/tcp.h>
#include <nanomsg/survey.h>

#include <signal.h>
#include "sqlite4.h"
#include <stdbool.h>

#include "freeq/libfreeq.h"
#include "libfreeq-private.h"

/* control */
#include "control/stralloc.h"
#include "control/constmap.h"
#include "control/control.h"
#include "control/qsutil.h"

#include <iostream>
#include <utility>
#include <map>
#include <vector>
#include <string>
#include <sstream>

/* sockserver */

#include <signal.h>

#define SERVER_PORT 13000
#define ERROR 1
#define SUCCESS 0
#define MAX_MSG 8192
int server;

/* end */

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

struct receiver_info {
	sqlite4 *pDb;
	const char *url;
};

/*const char *tables_sql = "SELECT name FROM sqlite_master WHERE type=\'table\';" */

typedef std::string tablename;
typedef std::string provider;
typedef std::map<tablename, struct freeq_table *> freeq_generation;

// struct {
// 	freeq_generation *gen;
// 	time_t era;
// 	struct freeq_gen_node *next;
// } freeq_gen_node;

// typedef struct freeq_gen_node freeq_gen_node_t;
// freeq_gen_node_t* generations;

//static void print_help (void);
//static void print_version (void);

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

// int aggregate_table_segments(struct freeq_ctx *ctx, const char *name, freeq_table *table)
// {
// 	typedef std::pair<std::string, int> colsig_t;
// 	typedef std::map<colsig_t, int> colprof_t;
// 	freeq_table *tbl, *parent, *tail;
// 	colprof_t colprof;
// 	int numcols = 0;

// 	dbg(ctx, "aggregating segments for table %s\n", name);

// 	tbl = table;
// 	while (tbl != NULL)
// 	{
// 		//dbg(ctx, "ok cool, we parsed %d segment headers.\n", headers.size());		
// 		numcols = 0;
// 		while (c != NULL)
// 		{
// 			colprof[colsig_t(c->name, c->coltype)]++;
// 			//c = c->next;
// 			numcols++;
// 		}

// 		tail->next = tbl;
// 		tail = tail->next;
// 		dbg(ctx, "segment %s:%s has %d columns\n", tbl->identity, tbl->name, numcols);
// 	}

// 	for (colprof_t::iterator it = colprof.begin(); it != colprof.end(); ++it)
// 	{
// 		if (it->second != numcols)
// 		{
// 			//consistant = false;
// 			dbg(ctx, "inconsistant schema!\n", "");
// 			break;
// 		}
// 	}

// 	// TODO: this is a bug
// 	tbl = parent->next;
// 	while (tbl != NULL)
// 	{
// 		//freeq_column *c = tbl->columns;
// 		//freeq_column *parc = parent->columns;
// 		numcols = 0;
// 		//while (c != NULL)
// 		//	parent->numrows += freeq_attach_all_segments(c, parc);
// 	}

// 	freeq_table_unref(parent->next);
// 	// *table = parent;
// 	return 0;
// }

int drop_table(struct freeq_table *tbl, sqlite4 *mDb)
{
	int res;
	char *drop_cmd;
	if (!asprintf(&drop_cmd, "DROP TABLE %s;", tbl->name))
	{
		fprintf(stderr, "failed to allocate buffer for drop table");
		return -ENOMEM;
	}
	res = sqlite4_exec(mDb, drop_cmd, NULL, NULL);
	free(drop_cmd);
	return res;
}

int create_table(struct freeq_ctx *ctx, struct freeq_table *tbl, sqlite4 *mDb)
{
	std::stringstream stm;
	stm << "CREATE TABLE IF NOT EXISTS " << tbl->name << "(";
	for (int i = 0; i < tbl->numcols; i++)
	{
		dbg(ctx, "create_table: type is %d\n", tbl->columns[i].coltype);
		stm << tbl->columns[i].name << " " << freeq_sqlite_typexpr[tbl->columns[i].coltype];
		if (i < (tbl->numcols - 1))		
			stm << ", ";
	}
	stm << ");";
	dbg(ctx, "%s\n", stm.str().c_str());
	return sqlite4_exec(mDb, stm.str().c_str(), NULL, NULL);
}

int statement(struct freeq_ctx *ctx, struct freeq_table *tbl, sqlite4 *mDb, sqlite4_stmt **stmt)
{
	sqlite4_stmt *s;
	std::stringstream stm;
	int res;

	stm << "INSERT INTO " << tbl->name << " VALUES (";
	for (int i = 1; i < tbl->numcols; i++)
		stm << "?" << i << ", ";

	stm << "?" << tbl->numcols << ");";
	dbg(ctx, "statement: %s\n", (char *)stm.str().c_str());
	res = sqlite4_prepare(mDb, stm.str().c_str(), stm.str().size(), &s, NULL);
	if (res == SQLITE4_OK)
		*stmt = s;

	return res;
}

int to_db(struct freeq_ctx *ctx, struct freeq_table *tbl, sqlite4 *mDb)
{
	sqlite4_stmt *stmt;
	const char **strarrp = NULL;
	int *intarrp = NULL;
	int res;

	if (sqlite4_exec(mDb, "BEGIN TRANSACTION;", NULL, NULL) != SQLITE4_OK)
	{
		dbg(ctx, "unable to start transaction\n");
		return 1;
	}

	if (drop_table(tbl, mDb) != SQLITE4_OK)
		dbg(ctx, "failed to drop table, ignoring");

	if (create_table(ctx, tbl, mDb) != SQLITE4_OK)
	{
		dbg(ctx, "failed to create table, rolling back");
		sqlite4_exec(mDb, "ROLLBACK;", NULL, NULL);
		return 1;
	}

	if ((res = statement(ctx, tbl, mDb, &stmt)) != SQLITE4_OK)
	{
		dbg(ctx, "failed to create statement (%d), rolling back", res);
		sqlite4_exec(mDb, "ROLLBACK;", NULL, NULL);
		return 1;
	}

	for (unsigned i = 0; i < tbl->numrows; i++)
	{
		for (int j = 0; j < tbl->numcols; j++) 
		{
			//strarrp = (const char **)seg->data;
			//intarrp = (int *)seg->data;
			switch (tbl->columns[i].coltype)
			{
			case FREEQ_COL_STRING:
				sqlite4_bind_text(stmt, j, strarrp[i], strlen(strarrp[i]), SQLITE4_TRANSIENT, NULL);
				if (res != SQLITE4_OK)
				{
					dbg(ctx, "failed bind: %s", sqlite4_errmsg(mDb));
				}
				break;
			case FREEQ_COL_NUMBER:
				res = sqlite4_bind_int(stmt, j, intarrp[i]);
				if (res != SQLITE4_OK)
				{
					dbg(ctx, "failed bind: %s", sqlite4_errmsg(mDb));
				}
				break;
			default:
				break;
			}
			//colp = colp->next;
			j++;
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

static
int log_monitor(void *NotUsed, int argc, sqlite4_value **argv, const char **azColName)
{
	int i;
	for (i = 0; i < argc; i++)
		printf("%s = %d\n", azColName[i], atoi((char *)argv[i]));

	printf("\n");
	return 0;
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
	// while (1) {
	//      sqlite4_exec(ri->p1Db, ".schema;", log_monitor, NULL);
	//      sleep(5);
	// }

}

/* runs in client mode, waits for an aggregator to request that we
 * join it's survey pool */
void *recvinvite(void *arg)
{
	const char *uri = (const char *)arg;
	freeq_ctx *ctx;
	int err;
	int sock = nn_socket(AF_SP, NN_PULL);

	err = freeq_new(&ctx, "appname", "identity");
	if (err)
	{
		dbg(ctx, "unable to create freeq context\n");
		return NULL;
	}

	freeq_set_log_priority(ctx, 10);
	dbg(ctx, "recvinvite attempting to bind %s\n", uri);

	assert(sock >= 0);
	if (nn_bind(sock, uri) < 1)
	{
		perror("error");
		exit(EXIT_FAILURE);
	}

	dbg(ctx, "recvinvite listener thread started\n");

	while (1)
	{
		char *buf = NULL;
		int size = nn_recv(sock, &buf, NN_MSG, 0);
		dbg(ctx, "read %d bytes\n", size);
	}

}

/* in client mode, an instance of this thread is started for each
 * aggregator that is surveying us, and waits for a survey request */
void *aggsurvey(void *arg)
{
	int err;
	const char *url = (const char *)arg;
	freeq_ctx *ctx;
	err = freeq_new(&ctx, "appname", "identity");
	if (err)
	{
		dbg(ctx, "unable to create freeq context");
		return NULL;
	}

	dbg(ctx, "starting aggregator thread");
	freeq_set_log_priority(ctx, 10);
	int sock = nn_socket(AF_SP, NN_RESPONDENT);
	assert (sock >= 0);
	assert (nn_connect (sock, url) >= 0);
	while (1)
	{
		char *buf = NULL;
		int bytes = nn_recv (sock, &buf, NN_MSG, 0);
		if (bytes >= 0)
		{
			//printf ("CLIENT (%s): RECEIVED \"%s\" SURVEY REQUEST\n", name, buf);
			nn_freemsg(buf);
			// char *d = date();
			// int sz_d = strlen(d) + 1; // '\0' too
			// printf ("CLIENT (%s): SENDING DATE SURVEY RESPONSE\n", name);
			// int bytes = nn_send (sock, d, sz_d, 0);
			// assert (bytes == sz_d);
		}
	}
	nn_shutdown(sock, 0);
}

/* in aggregator mode, send a survey request to our pool of clients */
void *aggregator(void *arg)
{
//	struct receiver_info *ri = (struct receiver_info *)arg;
	int err;
	freeq_ctx *ctx;
	err = freeq_new(&ctx, "appname", "identity");
	if (err)
	{
		dbg(ctx, "unable to create freeq context");
		return NULL;
	}

	dbg(ctx, "starting aggregator thread");
	freeq_set_log_priority(ctx, 10);

	int sock = nn_socket(AF_SP, NN_SURVEYOR);
	assert(sock >= 0);
	assert(nn_bind(sock, "tcp://*:13001") >= 0);
	sleep(1); // wait for connections

#define DATE "poop"

	int sz_d = strlen(DATE) + 1; // '\0' too
	printf ("SERVER: SENDING DATE SURVEY REQUEST\n");
	int bytes = nn_send(sock, DATE, sz_d, 0);
	assert(bytes == sz_d);

	while (1)
	{
		char *buf = NULL;
		int bytes = nn_recv(sock, &buf, NN_MSG, 0);
		if (bytes == ETIMEDOUT) break;
		if (bytes >= 0)
		{
			printf ("SERVER: RECEIVED \"%s\" SURVEY RESPONSE\n", buf);
			nn_freemsg (buf);
		}
	}
	nn_shutdown(sock, 0);
}



void *receiver (void *arg)
{
	struct receiver_info *ri = (struct receiver_info *)arg;
	freeq_ctx *ctx;
	int res;
	int err;
	int sock = nn_socket(AF_SP, NN_PULL);

	err = freeq_new(&ctx, "appname", "identity");
	if (err)
	{
		dbg(ctx, "unable to create freeq context");
		return NULL;
	}

	freeq_set_log_priority(ctx, 10);

	assert(sock >= 0);
	assert(nn_bind (sock, ri->url) >= 0);
	dbg(ctx, "freeqd receiver listening at %s\n", ri->url);

	freeq_generation fg;
	freeq_generation::iterator it;

	while (1)
	{
		char *buf = NULL;
		dbg(ctx, "generation has %d entries\n", fg.size());
		int size = nn_recv(sock, &buf, NN_MSG, 0);
		assert(size >= 0);

		freeq_table *table;
		dbg(ctx, "receiver(): read %d bytes\n", size);
		//res = freeq_table_header_from_msgpack(ctx, buf, size, &table);
		if (res)
		{
			dbg(ctx, "invalid header in message, rejecting\n");
			continue;
		}

		//dbg(ctx, "identity: %s name %s rows %d\n", table->identity, table->name, table->numrows);
		freeq_generation::iterator it = fg.find(table->name);
		if (it == fg.end())
		{
			dbg(ctx, "receiver: this is a new table\n");
			fg[table->name] = table;
		}
		else
		{
			struct freeq_table *tmp = it->second;
			struct freeq_table *tail = NULL;
			while (tmp != NULL)
			{
				if (tmp->identity == table->identity)
					break;
				tail = tmp;
				tmp = tmp->next;
			}

			/* we don't have a table from this provider */
			if (tmp == NULL)
			{
				dbg(ctx, "receiver: appending table for %s/%s\n", table->name, table->identity);
				tail->next = table;
			}
			else
			{
				dbg(ctx, "generation already contains %s/%s\n", table->name, table->identity);
			}
		}

		to_db(ctx, table, ri->pDb);
		//to_text(ctx, table);
		nn_freemsg(buf);
	}
}

void cleanup(int)
{
	fprintf(stderr, "received SIGINT, stopping server...");
	close(server);
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

void* handler(void *paramsd) {
	struct sockaddr_in cliAddr;
	char line[MAX_MSG];
	int res;
	int client_local;
	//int bsize;
	//const char *s;
	//char **strings;
	//int *vals;

	freeq_ctx *ctx;
	//struct freeq_table *tblp;
	//struct freeq_column *colp;
	//struct freeq_column_segment *seg;

	//sqlite4_stmt *pStmt;
	socklen_t addr_len;
	//int segment_size = 2000;

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

 	while(!recvstop)
 	{
// 		res = readline(client_local, line, MAX_MSG);
// 		dbg(ctx, "received query from client: \"%s\" res=%d\n", line, res);
// 		if (res == 0)
// 			break;

// 		if (res == 2)
// 			continue;

// 		res = sqlite4_prepare(pDb, line, -1, &pStmt, 0);
// 		if (res != SQLITE4_OK)
// 		{
// 			dbg(ctx, "prepare failed, sending error\n");
// 			freeq_error_write_sock(ctx, sqlite4_errmsg(pDb), client_local);
// 			sqlite4_finalize(pStmt);
// 			continue;
// 		}

// 		/* column type is unset until the query has been
// 		 * stepped once */
// 		if (sqlite4_step(pStmt) != SQLITE4_ROW)
// 		{
// 			freeq_error_write_sock(ctx, sqlite4_errmsg(pDb), client_local);
// 			sqlite4_finalize(pStmt);
// 			continue;
// 		}

// 		freeq_table_new_from_string(ctx, "result", &tblp);
// 		if (!tblp)
// 		{
// 			dbg(ctx, "unable to allocate table\n");
// 			freeq_error_write_sock(ctx, "ENOMEM", client_local);
// 			sqlite4_finalize(pStmt);
// 			continue;
// 		}

// 		int numcols = sqlite4_column_count(pStmt);
// 		dbg(ctx, "creating response table %s, %d columns\n", tblp->name, numcols);
// 		int j = 0;

// 		for (int i = 0; i < numcols; i++)
// 		{
// 			const char *cname = strdup(sqlite4_column_name(pStmt, i));
// 			int s4ctype = sqlite4_column_type(pStmt, i);
// 			freeq_coltype_t ctype = sqlite_to_freeq_coltype[s4ctype];
// 			dbg(ctx, "adding column %s sqlite4_type %d freeq_type %d\n", cname, s4ctype, ctype);
// 			res = freeq_table_column_new_empty(ctx, tblp, cname, ctype, &colp, segment_size);
// 			if (res < 0)
// 			{
// 				dbg(ctx, "unable to allocate column\n");
// 				freeq_error_write_sock(ctx, "ENOMEM", client_local);
// 				sqlite4_finalize(pStmt);
// 				freeq_table_unref(tblp);
// 				continue;
// 			}
// 		}

// 		do {
// 			if (j >= segment_size)
// 			{
// 				// pack and send table, flag continuation
// 				//send(client_local, "segment\n", 8, 0);
// 				tblp->numrows = 0;
// 				j = 0;
// 			}

// 			dbg(ctx, "handling row %d\n", j);
// 			colp = tblp->columns;
// 			for (int i = 0; i < numcols; i++)
// 			{
// 				seg = colp->segments;
// 				switch (colp->coltype) {
// 				case FREEQ_COL_STRING:
// 					strings = (char **)seg->data;
// 					s = sqlite4_column_text(pStmt, i, &bsize);
// 					strings[j] = strndup(s, bsize);
// 					break;
// 				case FREEQ_COL_NUMBER:
// 					vals = (int *)seg->data;
// 					vals[j] = sqlite4_column_int(pStmt, i);
// 					break;
// 				default:
// 					break;
// 				}
// 				colp = colp->next;
// 			}

// 			dbg(ctx, "setting all segments to len %d\n", j);
// 			colp = tblp->columns;
// 			while (colp != NULL)
// 			{
// 				colp->segments->len = j+1;
// 				colp = colp->next;
// 			}

// 			dbg(ctx, "setting table to len %d\n", j);
// 			tblp->numrows = j+1;
// 			j++;

// 		} while (sqlite4_step(pStmt) == SQLITE4_ROW);


// 		//dbg(ctx, "setting table, length %d\n", tblp->numrows);
// 		sqlite4_finalize(pStmt);
// 		// pack and send table, flag no continuation
// 		dbg(ctx, "sending result");
// 		freeq_table_write_sock(ctx, tblp, client_local);
// 		memset(line, 0, MAX_MSG);
// 		freeq_table_unref(tblp);
		sleep(1);
 	}
 	freeq_unref(ctx);
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
	servAddr.sin_port = htons(SERVER_PORT);
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
		dbg(ctx, "waiting for data on port %u\n", SERVER_PORT);
		addr_len = sizeof(cliAddr);
		client = accept(server, (struct sockaddr *) &cliAddr, &addr_len);
		if (client < 0)
		{
			dbg(ctx, "cannot accept connection ");
			break;
		}
		pthread_create(&thread, 0, &handler, &client);
	}
	close(server);
	freeq_unref(ctx);
	exit(0);
}

int
notify_clients(void)
{
	int i;
	int p;
	int err;
	char uribuf[64];
	char fmt[] = "tcp://%s:13000";
	struct freeq_ctx *ctx;

	//struct constmap mapclients;
	const char *clientfn = "control/clients";
	stralloc clients = {0};

	err = freeq_new(&ctx, "appname", "identity");
	if (err < 0)
		exit(EXIT_FAILURE);

	freeq_set_log_priority(ctx, 10);

	if (control_readfile(&clients,(char *)clientfn,1) != 1)
	{
		fprintf(stderr, "control/clients not found\n");
		return 0;
	} else {
		for (i = 0, p = i;i < clients.len; ++i) {
			if (!clients.s[i]) {
				if (snprintf((char *)&uribuf, 64, (char *)fmt, (char *)clients.s+p) <= 64)
				{
					int sock = nn_socket(AF_SP, NN_PUSH);
					assert(sock >= 0);
					assert(nn_connect(sock, uribuf) >= 0);
					dbg(ctx, "sent invite to %s\n", &uribuf);
					//fprintf(stderr, "sending %d bytes to %s\n", sbuf.size, url);
					//int bytes = nn_send(sock, sbuf.data, sbuf.size, 0);
					nn_shutdown(sock, 0);
				}
				p = i+1;
			}
		}
	}
	//while (!constmap_init(&mapclients,clients.s,clients.len,0)) nomem();
	//constmap_free(&mapclients);
}

int
main (int argc, char *argv[])
{
	int err;
	int res;

	//bool client_mode = false;
	//bool agg_mode = false;
	//bool freeql_mode = false;

	//const char *recvinvite_nnuri = "tcp://*:13002";
	sigset_t set;

	//pthread_t t_receiver;
	pthread_t t_aggregator;
	//pthread_t t_recvinvite;
	//pthread_t t_monitor;
	//pthread_t t_sqlserver;
	
	stralloc clients = {0};
	//stralloc aggport = {0};
	//stralloc cliport = {0};

	struct receiver_info ri;
	set_program_name(argv[0]);
	setlocale(LC_ALL, "");

#if ENABLE_NLS
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
#endif

	struct freeq_ctx *ctx;
	err = freeq_new(&ctx, "appname", "identity");
	if (err < 0)
		exit(EXIT_FAILURE);

	freeq_set_log_priority(ctx, 10);

	ri.pDb = pDb;
	ri.url = "ipc:///tmp/freeqd.ipc";

	sigemptyset(&set);
	sigaddset(&set, SIGHUP);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGUSR1);
	sigaddset(&set, SIGUSR2);
	sigaddset(&set, SIGALRM);
	pthread_sigmask(SIG_BLOCK, &set, NULL);

	// pthread_create(&t_monitor, 0, &monitor, (void *)&ri);

	if (control_readfile(&clients,(char *)"control/clients",1) != 1)
	{
		pthread_create(&t_aggregator, 0, &aggregator, (void *)&ri);
		//res = sqlite4_open(0, ":memory:", &pDb, 0);
		res = sqlite4_open(0, "ondisk.db", &pDb, SQLITE4_OPEN_READWRITE | SQLITE4_OPEN_CREATE,NULL);
		if (res != SQLITE4_OK)
		{
			fprintf(stderr, "failed to open in-memory db\n");
			exit(res);
		}
		notify_clients();
	}

	//if (control_readfile(&clients,"",1) != 1)
	//	pthread_create(&t_receiver, 0, &receiver, (void *)&ri);
	//	pthread_create(&t_recvinvite, 0, &recvinvite, (void *)recvinvite_nnuri);
	//}

	// if (freeql_mode)
	//	pthread_create(&t_sqlserver, 0, &sqlserver, (void *)&ri);

	res = sqlite4_exec(pDb, "create table if not exists freeq_stats(int last);", log_monitor, NULL);
	if (res != SQLITE4_OK)
	{
		printf("creating freeq_stats failed: %d %s\n", res, sqlite4_errmsg(pDb));
	}

	while (!recvstop)
	{
		//printf("running schema dump\n");
		//if (sqlite4_exec(pDb, "insert into poop values(1) ;", log_monitor, NULL) != SQLITE4_DONE) {
		//        printf("execute failed: %s\n", sqlite4_errmsg(pDb));
		//}
		sleep(5);
	}
	//return receiver(ctx, "ipc:///tmp/freeqd.ipc", pDb);
	//freeq_unref(&ctx);

}

// static void
// print_help (void)
// {
//   /* TRANSLATORS: --help output 1 (synopsis)
//      no-wrap */
//   printf (_("\
// Usage: %s [OPTION]...\n"), program_name);

//   /* TRANSLATORS: --help output 2 (brief description)
//      no-wrap */
//   fputs (_("\
// Print a friendly, customizable greeting.\n"), stdout);

//   puts ("");
//   /* TRANSLATORS: --help output 3: options 1/2
//      no-wrap */
//   fputs (_("\
//   -h, --help          display this help and exit\n\
//   -v, --version       display version information and exit\n"), stdout);

//   puts ("");
//   /* TRANSLATORS: --help output 4: options 2/2
//      no-wrap */
//   fputs (_("\
//   -t, --traditional       use traditional greeting\n\
//   -n, --next-generation   use next-generation greeting\n\
//   -g, --greeting=TEXT     use TEXT as the greeting message\n"), stdout);

//   printf ("\n");
//   /* TRANSLATORS: --help output 5+ (reports)
//      TRANSLATORS: the placeholder indicates the bug-reporting address
//      for this application.  Please add _another line_ with the
//      address for translation bugs.
//      no-wrap */
//   printf (_("\
// Report bugs to: %s\n"), PACKAGE_BUGREPORT);
// #ifdef PACKAGE_PACKAGER_BUG_REPORTS
//   printf (_("Report %s bugs to: %s\n"), PACKAGE_PACKAGER,
// 	  PACKAGE_PACKAGER_BUG_REPORTS);
// #endif
// #ifdef PACKAGE_URL
//   printf (_("%s home page: <%s>\n"), PACKAGE_NAME, PACKAGE_URL);
// #else
//   printf (_("%s home page: <http://www.gnu.org/software/%s/>\n"),
// 	  PACKAGE_NAME, PACKAGE);
// #endif
//   fputs (_("General help using GNU software: <http://www.gnu.org/gethelp/>\n"),
// 	 stdout);
// }
// 


// /* Print version and copyright information.  */

// static void
// print_version (void)
// {
//   printf ("%s (%s) %s\n", PACKAGE, PACKAGE_NAME, VERSION);
//   /* xgettext: no-wrap */
//   puts ("");

//   /* It is important to separate the year from the rest of the message,
//      as done here, to avoid having to retranslate the message when a new
//      year comes around.  */
//   printf (_("\
// Copyright (C) %d Free Software Foundation, Inc.\n\
// License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>\n\
// This is free software: you are free to change and redistribute it.\n\
// There is NO WARRANTY, to the extent permitted by law.\n"), COPYRIGHT_YEAR);
// }
