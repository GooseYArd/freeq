#include <config.h>
#include "system.h"
#include "progname.h"

#include <string.h>
#include <stdio.h>
#include <time.h>
#include <nanomsg/nn.h>
#include <nanomsg/pipeline.h>
#include <assert.h>

#include "freeq/libfreeq.h"
#include <proc/readproc.h>

#include "ccl/containers.h"

#define NODE0 "node0"
#define NODE1 "node1"

#define DEBUG(X) fprintf(stderr, _("DEBUG: %s\n"), X);

static const struct option longopts[] = {
        {"nodename", required_argument, NULL, 'n'},
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'v'},
        {NULL, 0, NULL, 0}
};

const freeq_coltype_t coltypes[] = {        
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

static void PrintStringCollection(strCollection *AL)
{
        size_t i;
        printf("Count %ld, Capacity %ld\n",
               (long)istrCollection.Size(AL),
               (long)istrCollection.GetCapacity(AL));
        for (i=0; i<istrCollection.Size(AL);i++) {
                printf("%s ",istrCollection.GetElement(AL,i));
        }
        printf("\n");
}


void procnothread(const char *machineip)
{
        struct freeq_ctx *ctx;
        struct freeq_table *tbl;
        proc_t proc_info;
        int err;

        err = freeq_new(&ctx, "system_monitor", NULL);
        if (err < 0)
                exit(EXIT_FAILURE);

        freeq_set_identity(ctx, machineip);

        strCollection *machineips = istrCollection.Create(2);
        strCollection *cmds = istrCollection.Create(2);
        ValArrayInt *pids = iValArrayInt.Create(100);
        ValArrayInt *pcpu = iValArrayInt.Create(100);
        ValArrayInt *state = iValArrayInt.Create(100);
        ValArrayInt *priority = iValArrayInt.Create(100);
        ValArrayInt *rss = iValArrayInt.Create(100);
        ValArrayInt *vsize = iValArrayInt.Create(100);
        ValArrayInt *euid = iValArrayInt.Create(100);
        ValArrayInt *egid = iValArrayInt.Create(100);
        ValArrayInt *ruid = iValArrayInt.Create(100);
        ValArrayInt *rgid = iValArrayInt.Create(100);
        
        PROCTAB* proc = openproc(PROC_FILLMEM | PROC_FILLSTAT | PROC_FILLSTATUS);

        memset(&proc_info, 0, sizeof(proc_info));

        while (readproc(proc, &proc_info) != NULL) {
                istrCollection.Add(machineips, "1.2.3.4");
                istrCollection.Add(cmds, proc_info.cmd);
                iValArrayInt.Add(pids, proc_info.ppid);
                iValArrayInt.Add(pcpu, proc_info.pcpu);
                iValArrayInt.Add(state, proc_info.state);
                iValArrayInt.Add(priority, proc_info.priority);
                iValArrayInt.Add(rss, proc_info.rss);
                iValArrayInt.Add(vsize, proc_info.vsize);
                iValArrayInt.Add(euid, proc_info.euid);
                iValArrayInt.Add(egid, proc_info.egid);
                iValArrayInt.Add(ruid, proc_info.ruid);
                iValArrayInt.Add(rgid, proc_info.rgid);
        }

        PrintStringCollection(cmds);

        err = freeq_table_new(ctx, 
                              "procnothread", 
                              12,
                              (freeq_coltype_t **)&coltypes, 
                              (char **)colnames, 
                              &tbl,
                              (char *)machineips,
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

        // if (err < 0)
        //         exit(EXIT_FAILURE);


        // freeq_table_send(ctx, tbl);
        // freeq_table_unref(tbl);
        // freeproctab(ptab);
        freeq_unref(ctx);
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
