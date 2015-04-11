#include <config.h>
#include "system.h"
#include "progname.h"

#include <string.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>

#include <netdb.h>
#include "freeq/libfreeq.h"
#include "libfreeq-private.h"
#include "ssl-common.h"

int
main (int argc, char *argv[])
{
  const char *node_name = _("localhost");
  struct freeq_table *tbl;
  char *sql;
  struct freeq_ctx *freeqctx;
  int err;

//  set_program_name(argv[0]);
  setlocale(LC_ALL, "");

#if ENABLE_NLS
  bindtextdomain(PACKAGE, LOCALEDIR);
  textdomain(PACKAGE);
#endif

  err = freeq_new(&freeqctx, "freeql", node_name, FREEQ_CLIENT);
  if (err < 0)
    exit(EXIT_FAILURE);

  freeq_set_identity(freeqctx, node_name);
  asprintf(&sql, "%s\r\n", argv[1]);

  if (freeq_ssl_query(freeqctx, "localhost:13000", sql, &tbl))
  {
    err(freeqctx, "some kind of error during query...\n");
  } else {
    freeq_table_print(freeqctx, tbl, stdout);
  }

  free(sql);
  exit (EXIT_SUCCESS);
}
