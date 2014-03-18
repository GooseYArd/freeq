#include <config.h>
#include "system.h"
#include "progname.h"

#include <assert.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <nanomsg/nn.h>
#include <nanomsg/pipeline.h>

#include "sqlite4.h"

#include "freeq/libfreeq.h"
#include "libfreeq-private.h"

#include <iostream>
#include <utility>
#include <map>
#include <vector>
#include <string>
#include <sstream>

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
	printf("IN CALLBACK...\n");
	for(i=0; i<argc; i++){
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
	bool consistant = true;

	freeq_table *tbl, *parent, *tail;
	colprof_t colprof;
	int numcols = 0;
	int sumrows = 0;
	int res;

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
			consistant = false;
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
	char* errorMessage;
	int len;
	const char *s;
	std::stringstream stm;
	struct freeq_column *col = tbl->columns;
	sqlite4_stmt *stmt;

	sqlite4_exec(mDb, "BEGIN TRANSACTION", callback, NULL);

	std::vector<struct freeq_column *> columns;
	int mVal = 0;
	int coli;

	stm << "DROP TABLE " << tbl->name;
	sqlite4_exec(mDb, stm.str().c_str(), callback, NULL);
	std::cout << stm.str() << std::endl;
	std::cout.flush();
	stm.str("");

	table_ddl(tbl, &stm);
	sqlite4_exec(mDb, stm.str().c_str(), callback, NULL);
	std::cout << stm.str() << std::endl;
	std::cout.flush();
	stm.str("");

	stm << "INSERT INTO example VALUES (";
	for (int i = 0; i < tbl->numcols; i++) {
		stm << "?" << i << ", ";
	}

	stm << tbl->numcols << ")";
	std::cout << stm.str() << std::endl;
	stm.clear();
	std::cout.flush();

	sqlite4_prepare(mDb, stm.str().c_str(), stm.str().size(), &stmt, NULL);
	for (unsigned i = 0; i < tbl->numrows; i++)
	{
		int j = 0;
		struct freeq_column *c = tbl->columns;
		struct freeq_column_segment *seg = c->segments;
		while (c != NULL) {
			switch (c->coltype)
			{
			case FREEQ_COL_STRING:
				len = strlen(((const char**)seg->data)[i]);
				s = ((const char **)seg->data)[i];
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
			printf("Commit Failed!\n");
		}
		sqlite4_reset(stmt);
		
	}
	sqlite4_exec(mDb, "COMMIT TRANSACTION", callback, NULL);
	sqlite4_finalize(stmt);
}

int receiver (struct freeq_ctx *ctx, const char *url, sqlite4 *pDb)
{
	int res;
	int sock = nn_socket(AF_SP, NN_PULL);
	assert(sock >= 0);
	assert(nn_bind (sock, url) >= 0);
	dbg(ctx, "freeqd receiver listening at %s\n", url);

	freeq_generation fg;
	freeq_generation::iterator it;

	while (1) {
		char *buf = NULL;
		size_t offset = 0;
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

		dbg(ctx, "identity: %s name %s\n", table->identity, table->name);
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
		to_db(ctx, table, pDb);
		nn_freemsg(buf);
	}
}

int
main (int argc, char *argv[])
{

  int optc;
  int lose = 0;
  int err;
  int res;

  sqlite4 *pDb;
  sqlite4_env *pEnv;
  char *pErrMsg = 0;

  const char *node_name = _("unknown");

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
      node_name = optarg;
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

  res = sqlite4_open(0, ":memory:", &pDb, 0);
  if (res !=SQLITE4_OK) {
    fprintf(stderr, "failed to open in-memory db\n");
    exit(res);
  }

  res = sqlite4_exec(pDb, "create table tbl1(one varchar(10), two smallint);", callback, NULL);
  if (res != SQLITE4_OK){
    fprintf(stderr, "SQL error: %s\n", pErrMsg);
    /* sqlite4_free(pErrMsg); */
  } else
    fprintf(stdout, "OK\n");

  res = sqlite4_exec(pDb, "insert into tbl1 values('hello!',10);", callback, NULL);
  if (res != SQLITE4_OK){
    fprintf(stderr, "SQL error: %s\n", pErrMsg);
    /* sqlite4_free(pErrMsg); */
  } else
    fprintf(stdout, "OK\n");

  res = sqlite4_exec(pDb, "insert into tbl1 values('goodbye', 20);", callback, NULL);
  if (res != SQLITE4_OK){
    fprintf(stderr, "SQL error: %s\n", pErrMsg);
    /* sqlite4_free(pErrMsg); */
  } else
    fprintf(stdout, "OK\n");

  res = sqlite4_exec(pDb, "select * from tbl1;", callback, NULL);
  if (res != SQLITE4_OK){
    fprintf(stderr, "SQL error: %s\n", pErrMsg);
    /* sqlite4_free(pErrMsg); */
  } else
    fprintf(stdout, "OK\n");

  return receiver(ctx, "ipc:///tmp/freeqd.ipc", pDb);

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
