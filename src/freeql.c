#include <config.h>
#include "system.h"
#include "progname.h"

#include <string.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>

#include <netdb.h>
//#include "freeq/libfreeq.h"
#include "libfreeq-private.h"
#include "ssl-common.h"

static const struct option longopts[] = {
        {"nodename", required_argument, NULL, 'n'},
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'v'},
        {NULL, 0, NULL, 0}
};

static void print_help (void);
static void print_version (void);

void
init_sockaddr (struct sockaddr_in *name,
               const char *hostname,
               uint16_t port)
{
  struct hostent *hostinfo;

  name->sin_family = AF_INET;
  name->sin_port = htons (port);
  hostinfo = gethostbyname (hostname);
  if (hostinfo == NULL)
  {
    fprintf (stderr, "Unknown host %s.\n", hostname);
    exit (EXIT_FAILURE);
  }
  name->sin_addr = *(struct in_addr *) hostinfo->h_addr;
}

int
main (int argc, char *argv[])
{
  int optc;
  int lose = 0;
  const char *node_name = _("localhost");
  struct freeq_table *tbl;
  char *sql;
  struct freeq_ctx *freeqctx;
  int err;

  set_program_name(argv[0]);
  setlocale(LC_ALL, "");

#if ENABLE_NLS
  bindtextdomain(PACKAGE, LOCALEDIR);
  textdomain(PACKAGE);
#endif

  while ((optc = getopt_long(argc, argv, "g:hq:v", longopts, NULL)) != -1)
    switch (optc)
    {
    case 'v':
      print_version();
      exit (EXIT_SUCCESS);
      break;
    case 'q':
      node_name = optarg;
      break;
    case 'h':
      print_help();
      exit (EXIT_SUCCESS);
      break;
    default:
      printf("set lose\n");
      lose = 1;
      break;
    }

  if (lose)
  {
    if (optind < argc)
      fprintf (stderr, _("%s: extra operand: %s\n"), program_name, argv[optind]);

    fprintf (stderr, _("Try `%s --help' for more information.\n"), program_name);
    exit (EXIT_FAILURE);
  }

  SSL_library_init();
  SSL_load_error_strings();

  err = freeq_new(&freeqctx, "tblsend", node_name);
  if (err < 0)
    exit(EXIT_FAILURE);

  freeq_set_identity(freeqctx, node_name);
  freeq_set_log_priority(freeqctx, 10);

  asprintf(&sql, "%s\r\n", argv[1]);

  if (freeq_ssl_query(freeqctx, "localhost:13002", sql, &tbl))
  {
    err(freeqctx, "some kind of error during query...\n");
  } else {
    freeq_table_print(freeqctx, tbl, stdout);
  }

  free(sql);

  exit (EXIT_SUCCESS);

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
  -n, --nodename=TEXT     use TEXT as the node name\n"), stdout);

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
