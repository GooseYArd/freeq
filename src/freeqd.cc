#include <config.h>
#include "system.h"
#include "progname.h"

#include <assert.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <nanomsg/nn.h>
#include <nanomsg/pipeline.h>
#include <signal.h>
#include "sqlite4.h"

#include "freeq/libfreeq.h"
#include "libfreeq-private.h"

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

static const struct option longopts[] = {
	{"nodename", required_argument, NULL, 'n'},
	{"help", no_argument, NULL, 'h'},
	{"version", no_argument, NULL, 'v'},
	{NULL, 0, NULL, 0}
};

static
int callback(void *NotUsed, int argc, sqlite4_value **argv, const char **azColName)
{
	int i;
	fprintf(stderr, "IN CALLBACK...\n");
	for (i = 0; i < argc; i++) {
		/* printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL"); */
		printf("%s\n", azColName[i]);
	}
	printf("\n");
	return 0;
}

typedef std::string tablename;
typedef std::string provider;
typedef std::map<tablename, struct freeq_table *> freeq_generation;

static void print_help (void);
static void print_version (void);

const char *freeq_sqlite_typexpr[] = {
	"VARCHAR(255)",
	"INTEGER",
	"INTEGER",
	"INTEGER",
	"INTEGER"
};

int aggregate_table_segments(struct freeq_ctx *ctx, const char *name, freeq_table *table) {

	typedef std::pair<std::string, int> colsig_t;
	typedef std::map<colsig_t, int> colprof_t;
	//bool consistant;
	freeq_table *tbl, *parent, *tail;
	colprof_t colprof;
	int numcols = 0;

	dbg(ctx, "aggregating segments for table %s\n", name);

	tbl = table;
	while (tbl != NULL) {
		//dbg(ctx, "ok cool, we parsed %d segment headers.\n", headers.size());
		freeq_column *c = tbl->columns;
		numcols = 0;
		while (c != NULL) {
			colprof[colsig_t(c->name, c->coltype)]++;
			c = c->next;
			numcols++;
		}

		tail->next = tbl;
		tail = tail->next;
		dbg(ctx, "segment %s:%s has %d columns\n", tbl->identity, tbl->name, numcols);
	}

	for (colprof_t::iterator it = colprof.begin(); it != colprof.end(); ++it) {
		if (it->second != numcols) {
			//consistant = false;
			dbg(ctx, "inconsistant schema!\n", "");
			break;
		}
	}

	tbl = parent->next;
	while (tbl != NULL) {
		freeq_column *c = tbl->columns;
		freeq_column *parc = parent->columns;
		numcols = 0;
		while (c != NULL) {
			parent->numrows += freeq_attach_all_segments(c, parc);
		}
	}

	freeq_table_unref(parent->next);
	// *table = parent;
	return 0;
}

int table_ddl(struct freeq_table *tbl, std::stringstream *stm) {
	struct freeq_column *col = tbl->columns;
	*stm << "CREATE TABLE " << tbl->name << "(";
	while (col != NULL) {
		*stm << col->name << " " << freeq_sqlite_typexpr[col->coltype];
		col = col->next;
		if (col != NULL)
			*stm << ", ";
	}
	*stm << ");";
}

int to_db(struct freeq_ctx *ctx, struct freeq_table *tbl, sqlite4 *mDb)
{
	int len;
	const char *s;
	std::stringstream stm;
	sqlite4_stmt *stmt;

	sqlite4_exec(mDb, "BEGIN TRANSACTION;", callback, NULL);

	stm << "DROP TABLE " << tbl->name << ";";
	sqlite4_exec(mDb, stm.str().c_str(), callback, NULL);

	std::cout << stm.str() << std::endl;
	std::cout.flush();
	stm.str("");

	table_ddl(tbl, &stm);
	sqlite4_exec(mDb, stm.str().c_str(), callback, NULL);
	std::cout << stm.str() << std::endl;
	std::cout.flush();
	stm.str("");

	stm << "INSERT INTO " << tbl->name << " VALUES (";
	for (int i = 0; i < tbl->numcols; i++) {
		stm << "?" << i << ", ";
	}

	stm << tbl->numcols << ");";
	std::cout << stm.str() << std::endl;
	stm.clear();
	std::cout.flush();

	std::cout << "statement prepared, executing for " << tbl->numrows << " rows"  << std::endl;
	sqlite4_prepare(mDb, stm.str().c_str(), stm.str().size(), &stmt, NULL);

	for (unsigned i = 0; i < tbl->numrows; i++)
	{
		int j = 0;
		struct freeq_column *c = tbl->columns;
		struct freeq_column_segment *seg = c->segments;		
		std::cout << "executing for row..." << std::endl;

		while (c != NULL) {
			switch (c->coltype)
			{
			case FREEQ_COL_STRING:
				len = strlen(((const char**)seg->data)[i]);
				s = ((const char **)seg->data)[i];
				std::cout << "STRING: " << s << std::endl;
				sqlite4_bind_text(stmt, j, s, len, SQLITE4_STATIC, NULL);
			case FREEQ_COL_NUMBER:
				sqlite4_bind_int(stmt, j, ((int *)seg->data)[i]);
			default:
				break;
			}
			j++;
			c = c->next;
		}

		if (sqlite4_step(stmt) != SQLITE4_DONE)
		{
			std::cout << "execute failed" << std::endl;
		}
		sqlite4_reset(stmt);
	}

	std::cout << "committing transaction" << std::endl;
	sqlite4_exec(mDb, "COMMIT TRANSACTION;", callback, NULL);
	sqlite4_finalize(stmt);
}

int to_text(struct freeq_ctx *ctx, struct freeq_table *tbl)
{
	const char **strarrp = NULL;
	struct freeq_column_segment *seg;
	int *intarrp = NULL;

	struct freeq_column *colp = tbl->columns;	
	while (colp != NULL) {
		printf("%s", colp->name);
		colp = colp->next;
		if (colp != NULL)
			printf(", ");
	}	
	printf("\n");

	for (unsigned i = 0; i < tbl->numrows; i++)
	{
		colp = tbl->columns;
		while (colp != NULL) {
			seg = colp->segments;
			strarrp = (const char **)seg->data;
			intarrp = (int *)seg->data;
			switch (colp->coltype)
			{
			case FREEQ_COL_STRING:
				printf("%s", strarrp[i]);
				break;
			case FREEQ_COL_NUMBER:
				printf("%d", intarrp[i]);
				break;
			default:
				break;
			}
			if (colp != NULL)
				printf(", ");
			colp = colp->next;			
		}
		printf("\n");
	}
}

static
int log_monitor(void *NotUsed, int argc, sqlite4_value **argv, const char **azColName)
{
	int i;
	//fprintf(stderr, "log_monitor, argc is %d\n", argc);
	for (i = 0; i < argc; i++) {		
		//printf("%s = %d\n", azColName[i], atoi((char *)argv[i]));
	}
	//printf("\n");
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
	while (pthread_info.num_active > 0) {
		pthread_cond_wait(&pthread_info.thread_exit_cv, &pthread_info.mutex);
	}

	pthread_mutex_unlock(&pthread_info.mutex);
	exit(0);
	return(NULL);

	// struct receiver_info *ri = (struct receiver_info *)arg;
	// freeq_ctx *ctx;
	// err = freeq_new(&ctx, "appname", "identity");
	// freeq_set_log_priority(ctx, 10);
	// while (1) {
	// 	sqlite4_exec(ri->p1Db, ".schema;", log_monitor, NULL);
	// 	sleep(5);
	// }

}

void *receiver (void *arg)
{
	struct receiver_info *ri = (struct receiver_info *)arg;
	freeq_ctx *ctx;
	int res;
	int err;
	int sock = nn_socket(AF_SP, NN_PULL);

	err = freeq_new(&ctx, "appname", "identity");
	if (err) {
		dbg(ctx, "unable to create freeq context");
		return NULL;
	}

	freeq_set_log_priority(ctx, 10);
	
	assert(sock >= 0);
	assert(nn_bind (sock, ri->url) >= 0);
	dbg(ctx, "freeqd receiver listening at %s\n", ri->url);

	freeq_generation fg;
	freeq_generation::iterator it;

	while (1) {
		char *buf = NULL;
		dbg(ctx, "generation has %d entries\n", fg.size());
		int size = nn_recv(sock, &buf, NN_MSG, 0);
		assert(size >= 0);

		freeq_table *table;
		dbg(ctx, "receiver(): read %d bytes\n", size);
		res = freeq_table_header_from_msgpack(ctx, buf, size, &table);
		if (res) {
			dbg(ctx, "invalid header in message, rejecting\n");
			continue;
		}

		dbg(ctx, "identity: %s name %s rows %d\n", table->identity, table->name, table->numrows);
		freeq_generation::iterator it = fg.find(table->name);
		if (it == fg.end())
		{
			dbg(ctx, "receiver: this is a new table\n");
			fg[table->name] = table;
		} else {

			struct freeq_table *tmp = it->second;
			struct freeq_table *tail = NULL;
			while (tmp != NULL) {
				if (tmp->identity == table->identity)
					break;
				tail = tmp;
				tmp = tmp->next;
			}

			/* we don't have a table from this provider */
			if (tmp == NULL) {
				dbg(ctx, "receiver: appending table for %s/%s\n", table->name, table->identity);
				tail->next = table;
			} else {
				dbg(ctx, "generation already contains %s/%s\n", table->name, table->identity);
			}

		}

		//to_db(ctx, table, ri->pDb);
		to_text(ctx, table);
		nn_freemsg(buf);
	}
}

void cleanup(int)
{
	close(server);
	pthread_exit(NULL);
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

	for (n = 1; n < maxlen; n++) {
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
	if (readline(fd, line, MAX_MSG) < 0)
		return ERROR; 
	return SUCCESS;
}

void* handler(void *paramsd) {
	struct sockaddr_in cliAddr;
	char line[MAX_MSG];
	int res;
	int client_local;
	sqlite4_stmt *pStmt;
	socklen_t addr_len;

	client_local = *((int *)paramsd);
	addr_len = sizeof(cliAddr);
    
	getpeername(client_local, (struct sockaddr*)&cliAddr, &addr_len);
	memset(line, 0, MAX_MSG);
    
	while(!recvstop && readnf(client_local, line) != ERROR)
	{		
		//strcpy(reply, "You:");
		//strcat(reply, line);
		send(client_local, "executing", 10, 0);		        
		//res = sqlite4_exec(pDb, line, callback, NULL);

		res = sqlite4_prepare(pDb, line, -1, &pStmt, 0);
		sqlite4_step(pStmt);
		res = sqlite4_column_int(pStmt, 0);
		sqlite4_finalize(pStmt);

		std::cout << "RAN statement " << line << " response was " << res << std::endl;
		
		send(client_local, "ok", 3, 0);		        
		memset(line,0,MAX_MSG);
	}     
	close(client_local);        
}


void *sqlserver(void *arg) {

	int client;
	socklen_t addr_len;
	pthread_t thread; 

	struct sockaddr_in cliAddr;
	struct sockaddr_in servAddr;

	signal(SIGINT, cleanup);
	signal(SIGTERM, cleanup);
	
	server = socket(PF_INET, SOCK_STREAM, 0);
	if (server < 0) {
		perror("cannot open socket ");
//		return ERROR;
		return NULL;
	}

	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servAddr.sin_port = htons(SERVER_PORT);
	memset(servAddr.sin_zero, 0, 8);
	
	if (bind(server, (struct sockaddr *) &servAddr, sizeof(struct sockaddr)) < 0) {
		perror("cannot bind port ");
//		return ERROR;
		return NULL;
	}

	listen(server, 5);

	while (!recvstop)
	{
		printf("waiting for data on port TCP %u\n", SERVER_PORT);
		addr_len = sizeof(cliAddr);
		client = accept(server, (struct sockaddr *) &cliAddr, &addr_len);
		if (client < 0) {
			perror("cannot accept connection ");
			break;
		}
		pthread_create(&thread, 0, &handler, &client);		
	}
	close(server);
	exit(0);
}


int
main (int argc, char *argv[])
{

  int optc;
  int lose = 0;
  int err;
  int res;
  sigset_t set;

  pthread_t t_receiver; 
  pthread_t t_monitor; 
  pthread_t t_sqlserver;

  struct receiver_info ri;  
  set_program_name(argv[0]);
  setlocale(LC_ALL, "");

#if ENABLE_NLS
  bindtextdomain(PACKAGE, LOCALEDIR);
  textdomain(PACKAGE);
#endif

  while ((optc = getopt_long (argc, argv, "g:hnv", longopts, NULL)) != -1)
    switch (optc)
    {
    case 'v':
      print_version ();
      exit (EXIT_SUCCESS);
      break;
    case 'n':
	    //node_name = optarg;
      break;
    case 'h':
      print_help ();
      exit (EXIT_SUCCESS);
      break;
    default:
      lose = 1;
      break;
    }

  if (lose || optind < argc)
  {
    if (optind < argc)
      fprintf (stderr, _("%s: extra operand: %s\n"), program_name,
	       argv[optind]);
    fprintf (stderr, _("Try `%s --help' for more information.\n"),
	     program_name);
    exit (EXIT_FAILURE);
  }

  struct freeq_ctx *ctx;
  err = freeq_new(&ctx, "appname", "identity");
  if (err < 0)
	  exit(EXIT_FAILURE);

  freeq_set_log_priority(ctx, 10);

  //res = sqlite4_open(0, ":memory:", &pDb, 0);
  res = sqlite4_open(0, "ondisk.db", &pDb, SQLITE4_OPEN_READWRITE|SQLITE4_OPEN_CREATE,NULL);

  if (res != SQLITE4_OK) {
    fprintf(stderr, "failed to open in-memory db\n");
    exit(res);
  }
	  
  ri.pDb = pDb;
  ri.url = "ipc:///tmp/freeqd.ipc";

  sigemptyset(&set);
  sigaddset(&set, SIGHUP);
  sigaddset(&set, SIGINT);
  sigaddset(&set, SIGUSR1);
  sigaddset(&set, SIGUSR2);
  sigaddset(&set, SIGALRM);
  pthread_sigmask(SIG_BLOCK, &set, NULL);

  pthread_create(&t_monitor, 0, &monitor, (void *)&ri);
  pthread_create(&t_receiver, 0, &receiver, (void *)&ri);
  pthread_create(&t_sqlserver, 0, &sqlserver, (void *)&ri);

  while (!recvstop) {
	  //printf("running schema dump\n");
	  if (sqlite4_exec(pDb, "select count(*) from procnothread;", log_monitor, NULL) != SQLITE4_DONE) {
		  //printf("execute failed: %s\n", sqlite4_errmsg(pDb));
	  }
	  sleep(5);
  }

  /* sqlserver(); */

  //return receiver(ctx, "ipc:///tmp/freeqd.ipc", pDb);
//  freeq_unref(&ctx);


}

static void
print_help (void)
{
  /* TRANSLATORS: --help output 1 (synopsis)
     no-wrap */
  printf (_("\
Usage: %s [OPTION]...\n"), program_name);

  /* TRANSLATORS: --help output 2 (brief description)
     no-wrap */
  fputs (_("\
Print a friendly, customizable greeting.\n"), stdout);

  puts ("");
  /* TRANSLATORS: --help output 3: options 1/2
     no-wrap */
  fputs (_("\
  -h, --help          display this help and exit\n\
  -v, --version       display version information and exit\n"), stdout);

  puts ("");
  /* TRANSLATORS: --help output 4: options 2/2
     no-wrap */
  fputs (_("\
  -t, --traditional       use traditional greeting\n\
  -n, --next-generation   use next-generation greeting\n\
  -g, --greeting=TEXT     use TEXT as the greeting message\n"), stdout);

  printf ("\n");
  /* TRANSLATORS: --help output 5+ (reports)
     TRANSLATORS: the placeholder indicates the bug-reporting address
     for this application.  Please add _another line_ with the
     address for translation bugs.
     no-wrap */
  printf (_("\
Report bugs to: %s\n"), PACKAGE_BUGREPORT);
#ifdef PACKAGE_PACKAGER_BUG_REPORTS
  printf (_("Report %s bugs to: %s\n"), PACKAGE_PACKAGER,
	  PACKAGE_PACKAGER_BUG_REPORTS);
#endif
#ifdef PACKAGE_URL
  printf (_("%s home page: <%s>\n"), PACKAGE_NAME, PACKAGE_URL);
#else
  printf (_("%s home page: <http://www.gnu.org/software/%s/>\n"),
	  PACKAGE_NAME, PACKAGE);
#endif
  fputs (_("General help using GNU software: <http://www.gnu.org/gethelp/>\n"),
	 stdout);
}



/* Print version and copyright information.  */

static void
print_version (void)
{
  printf ("%s (%s) %s\n", PACKAGE, PACKAGE_NAME, VERSION);
  /* xgettext: no-wrap */
  puts ("");

  /* It is important to separate the year from the rest of the message,
     as done here, to avoid having to retranslate the message when a new
     year comes around.  */
  printf (_("\
Copyright (C) %d Free Software Foundation, Inc.\n\
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>\n\
This is free software: you are free to change and redistribute it.\n\
There is NO WARRANTY, to the extent permitted by law.\n"), COPYRIGHT_YEAR);
}
