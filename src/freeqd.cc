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

typedef std::map<tablename, struct freeq_table *> freeq_generation;

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

int receiver (struct freeq_ctx *ctx, const char *url)
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
			if (tmp == NULL)
				tail->next = table;
			else {	
				dbg(ctx, "generation already contains %s/%s\n", table->name, table->identity);
			}

		}
		nn_freemsg(buf);
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
  err = freeq_new(&ctx, "appname", "identity");
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
