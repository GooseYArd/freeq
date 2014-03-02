#include <config.h>
#include "system.h"
#include "progname.h"

#include <assert.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <nanomsg/nn.h>
#include <nanomsg/pipeline.h>

#include <msgpack.hpp>
#include "freeq/libfreeq.h"
#include "libfreeq-private.h"

#include <iostream>
#include <vector>
#include <string>
#include <tuple>

static const struct option longopts[] = {
  {"nodename", required_argument, NULL, 'n'},
  {"help", no_argument, NULL, 'h'},
  {"version", no_argument, NULL, 'v'},
  {NULL, 0, NULL, 0}
};

class table {
private:
	std::string m_str;
	std::vector<int> m_vec;
public:
	//MSGPACK_DEFINE(m_str, m_vec);
};

typedef std::vector<char> msgbuf;
typedef std::tuple<std::string, std::string> tblmsgid;
typedef std::map<tblmsgid, msgbuf> freeq_generation;

static void print_help (void);
static void print_version (void);

char *date ()
{
  time_t raw = time (&raw);
  struct tm *info = localtime (&raw);
  char *text = asctime (info);
  text[strlen(text)-1] = '\0'; // remove '\n'
  return text;
}

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
	dbg(ctx, "freeqd receiver is ready on url %s", url);

	freeq_generation fg;
	freeq_generation::iterator it;

	while (1)
	{
		char *buf = NULL;
		size_t offset = 0;
		int size = nn_recv(sock, &buf, NN_MSG, 0);
		assert(size >= 0);

		unsigned char *b = NULL;
		int bsize = -1;

		freeq_table_header *header;
		dbg(ctx, "receiver(): read %d bytes\n", size);

		res = freeq_table_header_from_msgpack(ctx, buf, size, &header);
		if (res) {
			dbg(ctx, "invalid header in message, rejecting\n");
			continue;
		}

		dbg(ctx, "receiver: IDENTITY: %s TABLENAME %s\n", header->identity, header->tablename);
		tblmsgid tmid(header->identity, header->tablename);
		if (fg.find(tmid) == fg.end())
		{
			dbg(ctx, "receiver: this is a new producer\n");
			//fg[tmid] = msg;		
		} else {
			dbg(ctx, "current generation contains a block for this producer");
		}

		freeq_table_header_unref(ctx, header);		
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

  set_program_name (argv[0]);
  setlocale (LC_ALL, "");

#if ENABLE_NLS
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);
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
