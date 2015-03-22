#include <config.h>
#include "system.h"
#include "progname.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <string.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>

#include "freeq/libfreeq.h"
#include "libfreeq-private.h"
#include <proc/readproc.h>

/* control */
#include "control/stralloc.h"
#include "control/constmap.h"
#include "control/control.h"
#include "control/qsutil.h"

freeq_coltype_t coltypes[] = {
        FREEQ_COL_STRING, /* machineip */
        FREEQ_COL_STRING, /* command */
        FREEQ_COL_NUMBER, /* pid */
        FREEQ_COL_NUMBER, /* pcpu */
        FREEQ_COL_NUMBER, /* state */
        FREEQ_COL_NUMBER, /* priority */
        FREEQ_COL_NUMBER, /* nice */
        FREEQ_COL_NUMBER, /* rss */
        FREEQ_COL_NUMBER, /* vsize */
        FREEQ_COL_NUMBER, /* euid */
        FREEQ_COL_NUMBER, /* egid */
        FREEQ_COL_NUMBER, /* ruid */
        FREEQ_COL_NUMBER  /* rgid */
};

const char *colnames[] = {
        "machineip",
        "command",
        "pid",
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

void
freeproctab(proc_t ** tab)
{
        proc_t** p;
        for(p = tab; *p; p++)
                freeproc(*p);
        free(tab);
}

void
procnothread(struct freeq_ctx *ctx, const char *machineip)
{
        struct freeq_table *tbl;
        proc_t proc_info;
        int err;

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
                cmds        =  g_slist_append(cmds,        strdup(proc_info.cmd));
                pids        =  g_slist_append(pids,        GINT_TO_POINTER(proc_info.ppid));
                pcpu        =  g_slist_append(pcpu,        GINT_TO_POINTER(proc_info.pcpu));
                state       =  g_slist_append(state,       GINT_TO_POINTER(proc_info.state));
                priority    =  g_slist_append(priority,    GINT_TO_POINTER(proc_info.priority));
                nice        =  g_slist_append(nice,        GINT_TO_POINTER(proc_info.nice));
                rss         =  g_slist_append(rss,         GINT_TO_POINTER(proc_info.rss));
                vsize       =  g_slist_append(vsize,       GINT_TO_POINTER(proc_info.vsize));
                euid        =  g_slist_append(euid,        GINT_TO_POINTER(proc_info.euid));
                egid        =  g_slist_append(egid,        GINT_TO_POINTER(proc_info.egid));
                ruid        =  g_slist_append(ruid,        GINT_TO_POINTER(proc_info.ruid));
                rgid        =  g_slist_append(rgid,        GINT_TO_POINTER(proc_info.rgid));
                machineips  =  g_slist_append(machineips,  (char *)machineip);
        }

        err = freeq_table_new(ctx,
                              "procnothread",
                              13,
                              (freeq_coltype_t *)&coltypes,
                              (const char **)&colnames,
                              &tbl,
                              0,
                              machineips,
                              cmds,
                              pids,
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
        {
                err(ctx, "unable to create table\n");
                exit(EXIT_FAILURE);
        }
        //freeq_table_print(ctx, tbl, stdout);
        err = freeq_table_sendto_ssl(ctx, tbl);
        dbg(ctx, "freeq_table_sendto_ssl returned %d\n", err);

        freeq_table_unref(tbl);
        closeproc(proc);
        freeq_unref(ctx);
}

int
main(int argc, char *argv[])
{
        struct freeq_ctx *ctx;
        int err;

        set_program_name(argv[0]);
        setlocale(LC_ALL, "");

#if ENABLE_NLS
        bindtextdomain (PACKAGE, LOCALEDIR);
        textdomain (PACKAGE);
#endif
        static stralloc identity = {0};

        err = freeq_new(&ctx, "system_monitor", NULL, FREEQ_CLIENT);
        if (err < 0)
                exit(EXIT_FAILURE);

        err = control_readline(&identity, "control/identity");
        if (!err)
        {
                err(ctx, "unable to read control/identity");
                exit(FREEQ_ERR);
        }

        stralloc_0(&identity);
        freeq_set_identity(ctx, identity.s);
        freeq_set_log_priority(ctx, 10);

        procnothread(ctx, identity.s);

        return EXIT_SUCCESS;
}
