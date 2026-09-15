/* gate: hermes_cli_backup_py__restore_cron_jobs_if_emptied — traced copy of backup_restore_cron_jobs_if_emptied */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int backup_restore_cron_jobs_if_emptied(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("backup_restore_cron_jobs_if_emptied");
    int result = backup_restore_cron_jobs_if_emptied(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("backup_restore_cron_jobs_if_emptied");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s backup_restore_cron_jobs_if_emptied <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
