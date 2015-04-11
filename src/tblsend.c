#include <config.h>
#include "system.h"
#include "progname.h"
#include <stdbool.h>

#include <freeq/libfreeq.h>
#include "libfreeq-private.h"

#include <glib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <string.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>

#include "freeq/libfreeq.h"
#include "libfreeq-private.h"

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#define NODE0 "node0"
#define NODE1 "node1"

#define DEBUG(X) fprintf(stderr, _("DEBUG: %s\n"), X);

static const struct option longopts[] = {
        {"nodename", required_argument, NULL, 'n'},
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'v'},
        {NULL, 0, NULL, 0}
};

static void print_help (void);
static void print_version (void);

char * trim(char *s) {
        while (*s == ' ')
                s++;
        size_t len = strlen(s);

        if (len && ((s[len-1] == '\n') || (s[len-1] == '\r'))) {
                s[len-1] = '\0';
        }
        return s;
}

int countcols(FILE *f)
{
        char *lbuf = NULL;
        char *p;
        ssize_t r=0;
        size_t rn=0;
        int n = -1;

        if ((r = getline(&lbuf, &rn, f)) > 0)
        {
                n = 1;
                p = lbuf;
                while (*p) n += *(p++) == ',';
        }

        fseek(f, -r, SEEK_CUR);
        free(lbuf);
        return n;
}

int readserial(FILE *f, int *serial)
{
        size_t r = 0;
        char *lbuf = NULL;
        int res = 0;

        if (getline(&lbuf, &r, f))
        {
                if (sscanf(trim(lbuf), "%d", serial) != 1)
                        res = -1;
        } else {
                res = -1;
        }
        free(lbuf);
        return res;
}

int readname(FILE *f, char **name)
{
        size_t n = 0;
        ssize_t r = 0;
        char *lbuf = NULL;

        int res = 0;
        if ((r = getline(&lbuf, &n, f)) > 0)
        {
                *name = strdup(trim(lbuf));
        } else {
                res = -1;
        }
        return res;
}


int readcolnames(FILE *f, struct freeq_table *tbl)
{
        char *lbuf = NULL;
        char *p = NULL;
        size_t r = 0;
        ssize_t n = 0;

        n = getline(&lbuf, &r, f);
        if (n < 1)
                return -1;

        p = lbuf;
        for (int j = 0; j < tbl->numcols; j++)
                tbl->columns[j].name = strdup(trim(strtok(j == 0 ? p : NULL, ",")));

        free(lbuf);
        return 0;
}

int readcoldata(FILE *f, struct freeq_table *tbl)
{
        size_t r;
        char *tok;
        char *lbuf = NULL;
        bool err;
        uint64_t nval;

        GSList **coldata = calloc(sizeof(GSList *), tbl->numcols);
        if (coldata == NULL)
        {
                dbg(tbl->ctx, "unable to allocate coldata lists\n");
                return -ENOMEM;
        }

        while (getline(&lbuf, &r, f) > 0 && !err)
        {
                for (int j = 0; j < tbl->numcols; j++)
                {
                        tok = strtok(j == 0 ? lbuf : NULL, ",");
                        switch (tbl->columns[j].coltype) {
                        case FREEQ_COL_STRING:
                                coldata[j] = g_slist_prepend(coldata[j], strdup(tok));
                                break;
                        case FREEQ_COL_NUMBER:
                                if (sscanf(tok, "%lu", &nval) != 1)
                                        err = 1;
                                else
                                        coldata[j] = g_slist_prepend(coldata[j], GUINT_TO_POINTER(nval));
                                break;
                        default:
                                break;
                        }
                }
                tbl->numrows++;
        }

        if (err)
        {
                for (int j = 0; j < tbl->numcols; j++)
                {
                        if (coldata[j] != NULL)
                                g_slist_free_full(coldata[j], g_free);
                        return -1;
                }
        } else {
                for (int j = 0; j < tbl->numcols; j++)
                        tbl->columns[j].data = g_slist_reverse(coldata[j]);
        }

        free(lbuf);
        return 0;
}

freeq_coltype_t coltype_from_str(const char *tok)
{
        if (strcasecmp(tok, "null") == 0)
                return FREEQ_COL_NULL;
        else if (strcasecmp(tok, "string") == 0)
                return FREEQ_COL_STRING;
        else if (strcasecmp(tok, "number") == 0)
                return FREEQ_COL_NUMBER;
        else if (strcasecmp(tok, "time") == 0)
                return FREEQ_COL_TIME;
        else if (strcasecmp(tok, "ipv4_addr") == 0)
                return FREEQ_COL_IPV4ADDR;
        else if (strcasecmp(tok, "ipv6_addr") == 0)
                return FREEQ_COL_IPV6ADDR;
        else
                return -1;
}

int readcoltypes(FILE *f, struct freeq_table *tbl)
{
        size_t r;
        char *lbuf = NULL;
        char *tok;

        if (getline(&lbuf, &r, f) < 1)
                return -1;

        for (int j = 0; j < tbl->numcols; j++)
        {
                tok = trim(strtok(j == 0 ? lbuf : NULL, ","));

                tbl->columns[j].coltype = coltype_from_str(tok);
                if (tbl->columns[j].coltype == 255)
                        return -1;
        }

        if ((tok = strtok(NULL, ",")) != NULL)
                fprintf(stderr, "too many columns, tok %s\n", tok);

        free(lbuf);
        return 0;
}

void pubtbl(const char *fn)
{
        struct freeq_ctx *ctx;
        struct freeq_table *tbl;
        int numcols=1;
        int serial;
        char *tblname;
        int err;
        GStringChunk *strchnk;

        err = freeq_new(&ctx, "system_monitor", "tblsend", FREEQ_CLIENT);
        if (err < 0)
                exit(EXIT_FAILURE);

        freeq_set_identity(ctx, fn);
        freeq_set_log_priority(ctx, 10);

        dbg(ctx, "created context, opening file %s\n", fn);
        FILE *f = fopen(fn, "r");
        if (f == NULL)
                exit(EXIT_FAILURE);

        dbg(ctx, "reading serial\n");
        if (readserial(f, &serial)) {
                err(ctx, "invalid serial\n");
                exit(EXIT_FAILURE);
        }

        dbg(ctx, "serial is %d\n", serial);
        if (readname(f, &tblname))
                exit(EXIT_FAILURE);
        dbg(ctx, "name is %s\n", tblname);

        numcols = countcols(f);
        dbg(ctx, "%d columns\n", numcols);

        strchnk = g_string_chunk_new(64);
        err = freeq_table_new_fromcols(ctx,
                                       tblname,
                                       numcols,
                                       &tbl,
                                       strchnk,
                                       true);
        if (err < 0)
                exit(EXIT_FAILURE);

        tbl->serial = serial;
        dbg(ctx, "allocated table, serial %d\n", serial);
        if (readcolnames(f, tbl) != 0)
                exit(EXIT_FAILURE);

        dbg(ctx, "got column names\n");
        if (readcoltypes(f, tbl) != 0)
                exit(EXIT_FAILURE);

        dbg(ctx, "got column types\n");

        if (readcoldata(f, tbl) != 0)
        {
                freeq_table_unref(tbl);
                exit(EXIT_FAILURE);
        }

        //freeq_table_print(ctx, tbl, stdout);

        freeq_table_sendto_ssl(ctx, tbl);

        /* BIO *out, *in; */
        /* out = BIO_new_file("poop2.txt", "w"); */
        /* freeq_table_bio_write(ctx, tbl, out); */
        /* BIO_free(out); */

        /* in = BIO_new_file("poop2.txt", "r"); */
        /* freeq_table_bio_read(ctx, &t2, in, NULL); */
        /* BIO_free(in); */

        /* freeq_table_print(ctx, t2, stdout); */

        freeq_table_unref(tbl);
        //freeq_table_unref(t2);
        freeq_unref(ctx);
}

int
main (int argc, char *argv[])
{
  int optc;
  int lose = 0;
  //const char *node_name = _("unknown");
  const char *fn;

//  set_program_name (argv[0]);
  setlocale (LC_ALL, "");

#if ENABLE_NLS
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);
#endif

  while ((optc = getopt_long(argc, argv, "g:hn:i:v", longopts, NULL)) != -1)
    switch (optc)
    {
    case 'v':
      print_version();
      exit (EXIT_SUCCESS);
      break;
    case 'n':
            //node_name = optarg;
      break;
    case 'i':
      fn = optarg;
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
          /* if (optind < argc) */
          /*         fprintf (stderr, _("%s: extra operand: %s\n"), program_name, */
          /*                  argv[optind]); */
          /* fprintf (stderr, _("Try `%s --help' for more information.\n"), */
          /*          program_name); */
          exit (EXIT_FAILURE);
  }

  DEBUG("starting publisher");

  pubtbl(fn);
  return EXIT_SUCCESS;

}

static void
print_help (void)
{
  /* TRANSLATORS: --help output 1 (synopsis)
     no-wrap */
/*  printf (_("                                 \
    Usage: %s [OPTION]...\n"), program_name); */

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
