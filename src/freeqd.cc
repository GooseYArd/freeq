#include <config.h>
#include "system.h"
#include "progname.h"

#include <assert.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <nanomsg/nn.h>
#include <nanomsg/pipeline.h>

//#include <msgpack.hpp>

#include "freeq/libfreeq.h"
#include "libfreeq-private.h"

#include <iostream>
#include <utility>
#include <map>
#include <vector>
#include <string>

static const struct option longopts[] = {
	{"nodename", required_argument, NULL, 'n'},
	{"help", no_argument, NULL, 'h'},
	{"version", no_argument, NULL, 'v'},
	{NULL, 0, NULL, 0}
};

typedef std::string tablename;
typedef std::string provider;

typedef std::pair<int, char *> msgbuf;
typedef std::map<provider, msgbuf> table_segments;
typedef std::map<tablename, table_segments> freeq_generation;

static void print_help (void);
static void print_version (void);

/*
   how this should work
   if we are freeqd:
   we get a serialized message
   we deserialize the message and read the name of the table

   we check our generation map
   if we have an entry for this identity and this tablename, we replace it with this message
   if we do not have an entry for this identity and tablename, we store this message

   if we are a region leader:
   we get a serialized message
   we deserialize the message and read the name of the table

*/

int aggregate_table_segments(struct freeq_ctx *ctx, const char *name, table_segments *ts, freeq_table **table) {
		
	std::vector<freeq_table *> headers;
	typedef std::pair<std::string, int> colsig_t;
	typedef std::map<colsig_t, int> colprof_t;
	bool consistant = true;

	freeq_table *tbl;

	colprof_t colprof;
	int numcols = 0;
	int sumrows = 0;
	int res;
	
	dbg(ctx, "aggregating segments for table %s\n", name);

	// first, determine if the column set is uniform among our publishers
	for (table_segments::iterator it = ts->begin(); it != ts->end(); ++it) {
		const std::string prov = it->first;
		const msgbuf mb = it->second;
		const size_t size = mb.first;
		char *buf = mb.second;		
		freeq_table *tbl;
		//dbg(ctx, "parsing header for provider %s\n", prov.c_str());
		res = freeq_table_header_from_msgpack(ctx, buf, size, &tbl);
		
		sumrows += tbl->numrows;
		//dbg(ctx, "ok cool, we parsed %d segment headers.\n", headers.size());	
		freeq_column *c = tbl->columns;
		numcols = 0;
		while (c != NULL) {
			colprof[colsig_t(c->name, c->coltype)]++;
			c = c->next;
			numcols++;
		}
		dbg(ctx, "segment %s:%s has %d columns\n", tbl->identity, tbl->name, numcols);
	}
	
	for (colprof_t::iterator it = colprof.begin(); it != colprof.end(); ++it) {
		if (it->second != numcols) {
			consistant = false;
			dbg(ctx, "inconsistant schema!\n", "");
			break;
		}
	}
	
	res = freeq_table_new_from_string(ctx, name, &tbl);
	

}

int unpack_table(struct freeq_ctx *ctx, char *buf)
{
// for (unsigned int i = 2; i < obj.via.array.size; i++) {
	//	printf("reading row %d\n", i-2);
		//	msgpack_object o = obj.via.array.ptr[i];
		//	printf("ROW SIZE: %d\n", o.via.array.size);
		//	switch (o.type) {
		//	case 0:
		//		//EXPECT_EQ(MSGPACK_OBJECT_NIL, o.type);
		//		break;
		//	case 1:
		//		//EXPECT_EQ(MSGPACK_OBJECT_BOOLEAN, o.type);
		//		//EXPECT_EQ(true, o.via.boolean);
		//		break;
		//	case 2:
		//		//EXPECT_EQ(MSGPACK_OBJECT_BOOLEAN, o.type);
		//		//EXPECT_EQ(false, o.via.boolean);
		//		break;
		//	case 3:
		//		printf("INTEGER!!!");
		//		//EXPECT_EQ(MSGPACK_OBJECT_POSITIVE_INTEGER, o.type);
		//		//EXPECT_EQ(10, o.via.u64);
		//		break;
		//	case 4:
		//		printf("NEGATIVE INTEGER!!!");
		//		//EXPECT_EQ(MSGPACK_OBJECT_NEGATIVE_INTEGER, o.type);
		//		//EXPECT_EQ(-10, o.via.i64);
		//		break;
		//	}
		// }
}

int receiver (struct freeq_ctx *ctx, const char *url)
{
	int res;
	int sock = nn_socket(AF_SP, NN_PULL);
	assert(sock >= 0);
	assert(nn_bind (sock, url) >= 0);
	dbg(ctx, "freeqd receiver listening at %s\n", url);

	freeq_generation fg;
	freeq_generation::iterator it;

	while (1)
	{
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
			fg[std::string(table->name)][std::string(table->identity)] = msgbuf(size, buf);
		} else {
			table_segments ts = it->second;
			table_segments::iterator jt = ts.find(table->identity);
			dbg(ctx, "not a new table\n");
			if (jt == ts.end()) {
				dbg(ctx, "provider %s hasn't given us rows for %s\n", table->identity, table->name);
				fg[table->name][table->identity] = msgbuf(size, buf);
			} else {
				dbg(ctx, "provider %s already gave us some rows for %s\n", table->identity, table->name);
			}
			
		}

		//freeq_table_header_unref(ctx, table);

		for (it = fg.begin(); it != fg.end(); it++) {
			freeq_table *t;
			const char *name = it->first.c_str();
			//res = freeq_table_new_from_string(ctx, it->first.c_str(), &t);
			res = aggregate_table_segments(ctx, name, &it->second, &t); 
		}

	}
}

int
main (int argc, char *argv[])
{

  int optc;
  int lose = 0;
  int err;
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
  err = freeq_new(&ctx);
  if (err < 0)
	  exit(EXIT_FAILURE);

  freeq_set_log_priority(ctx, 10);
  return receiver(ctx, "ipc:///tmp/freeqd.ipc");

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
