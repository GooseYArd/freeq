#include <config.h>
#include "system.h"
#include "progname.h"

#include <assert.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <nanomsg/nn.h>
#include <nanomsg/pipeline.h>

#define DEBUG(X) fprintf(stderr, _("DEBUG: %s\n"), X);

static const struct option longopts[] = {
  {"nodename", required_argument, NULL, 'n'},
  {"help", no_argument, NULL, 'h'},
  {"version", no_argument, NULL, 'v'},
  {NULL, 0, NULL, 0}
};

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

int receiver (const char *url)
{
  int sock = nn_socket (AF_SP, NN_PULL);
  assert (sock >= 0);
  assert (nn_bind (sock, url) >= 0);
  DEBUG("freeqd receiver is ready");
  while (1)
    {
      char *buf = NULL;
      int bytes = nn_recv (sock, &buf, NN_MSG, 0);
      assert (bytes >= 0);
      DEBUG("got message");
      nn_freemsg (buf);
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


  return receiver("ipc:///tmp/freeqd.ipc");

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
