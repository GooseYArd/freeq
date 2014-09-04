#include <config.h>
#include "system.h"
#include "progname.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <string.h>
#include <stdio.h>
#include <time.h>
#include <nanomsg/nn.h>
#include <nanomsg/pipeline.h>
#include <assert.h>

#include "freeq/libfreeq.h"
#include <proc/readproc.h>

#define NODE0 "node0"
#define NODE1 "node1"

#define DEBUG(X) fprintf(stderr, _("DEBUG: %s\n"), X);

static const struct option longopts[] = {
        {"nodename", required_argument, NULL, 'n'},
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'v'},
        {NULL, 0, NULL, 0}
};

freeq_coltype_t coltypes[] = {        
        FREEQ_COL_STRING,
        FREEQ_COL_NUMBER,
        FREEQ_COL_STRING,
        FREEQ_COL_NUMBER,
        FREEQ_COL_NUMBER,
        FREEQ_COL_NUMBER,
        FREEQ_COL_NUMBER,
        FREEQ_COL_NUMBER,
        FREEQ_COL_NUMBER,
        FREEQ_COL_NUMBER,
        FREEQ_COL_NUMBER,
        FREEQ_COL_NUMBER,
        FREEQ_COL_NUMBER
};

const char *colnames[] = {
        "machineip",
        "pid",      
        "command",  
        "pcpu",     
        "state",    
        "priority", 
        "nice",     
        "rss",      
        "vsize",    
        "euid",     
        "egid",     
        "ruid",     
        "rgid"
};

static void print_help (void);
static void print_version (void);

void freeproctab(proc_t ** tab)
{
  proc_t** p;
  for(p = tab; *p; p++)
    freeproc(*p);
  free(tab);
}

void procnothread(const char *machineip)
{
        struct freeq_ctx *ctx;
        struct freeq_table *tbl;
        proc_t proc_info;
        int err;
        
        fprintf(stderr, "machine ip is %s\n", machineip);

        err = freeq_new(&ctx, "system_monitor", machineip);
        if (err < 0)
                exit(EXIT_FAILURE);

        freeq_set_identity(ctx, machineip);

        GSList *machineips = NULL, 
                *cmds = NULL, 
                *pids = NULL, 
                *pcpu = NULL, 
                *state = NULL, 
                *priority = NULL,
                *nice = NULL,
                *rss = NULL, 
                *vsize = NULL, 
                *euid = NULL, 
                *egid = NULL, 
                *ruid = NULL, 
                *rgid = NULL;        
        PROCTAB* proc = openproc(PROC_FILLMEM | PROC_FILLSTAT | PROC_FILLSTATUS);
        memset(&proc_info, 0, sizeof(proc_info));
       
        while (readproc(proc, &proc_info) != NULL) {
                cmds = g_slist_append(cmds, proc_info.cmd);
                pids = g_slist_append(pids, GINT_TO_POINTER(proc_info.ppid));
                pcpu = g_slist_append(pcpu, GINT_TO_POINTER(proc_info.pcpu));
                state = g_slist_append(state, GINT_TO_POINTER(proc_info.state));
                priority = g_slist_append(priority, GINT_TO_POINTER(proc_info.priority));
                nice = g_slist_append(nice, GINT_TO_POINTER(proc_info.nice));
                rss = g_slist_append(rss, GINT_TO_POINTER(proc_info.rss));
                vsize = g_slist_append(vsize, GINT_TO_POINTER(proc_info.vsize));
                euid = g_slist_append(euid, GINT_TO_POINTER(proc_info.euid));
                egid = g_slist_append(egid, GINT_TO_POINTER(proc_info.egid));
                ruid = g_slist_append(ruid, GINT_TO_POINTER(proc_info.ruid));
                rgid = g_slist_append(rgid, GINT_TO_POINTER(proc_info.rgid));
                machineips = g_slist_append(machineips, "1.2.3.4");
        }

        err = freeq_table_new(ctx, 
                              "procnothread", 
                              12,
                              (freeq_coltype_t *)&coltypes, 
                              (const char **)&colnames, 
                              1,
                              &tbl,
                              machineips,
                              pids,
                              cmds,
                              pcpu,
                              state,
                              priority,
                              nice,
                              rss,
                              vsize, 
                              euid,
                              egid, 
                              ruid,
                              rgid);

        if (err < 0)
                exit(EXIT_FAILURE);
 
        //freeq_table_write(ctx, tbl, f);
        //close(f);

        freeq_table_unref(tbl);
        //closeproc(proc);        
        //freeq_unref(ctx);
}

int publisher (const char *url, const char *agent, const char *node_name)
{
  procnothread(node_name);
}

int
main (int argc, char *argv[])
{
  int optc;
  int lose = 0;
  const char *node_name = _("unknown");

  set_program_name (argv[0]);
  setlocale (LC_ALL, "");

#if ENABLE_NLS
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);
#endif

  while ((optc = getopt_long(argc, argv, "g:hn:v", longopts, NULL)) != -1)
    switch (optc)
    {
    case 'v':
      print_version();
      exit (EXIT_SUCCESS);
      break;
    case 'n':
      node_name = optarg;
      break;
    case 'h':
      print_help();
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

  DEBUG("starting publisher");

  return publisher("ipc:///tmp/freeqd.ipc", "system_monitor", node_name);
  return EXIT_SUCCESS;

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
