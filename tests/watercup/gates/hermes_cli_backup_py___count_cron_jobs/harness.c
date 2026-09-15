/* gate: hermes_cli_backup_py___count_cron_jobs — traced copy of backup_count_cron_jobs */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int backup_count_cron_jobs(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("backup_count_cron_jobs");
    int result = backup_count_cron_jobs(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("backup_count_cron_jobs");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s backup_count_cron_jobs <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
