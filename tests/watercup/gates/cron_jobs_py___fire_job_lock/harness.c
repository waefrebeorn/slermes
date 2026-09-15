/* gate: cron_jobs_py___fire_job_lock — traced copy of cronjobs_fire_job_lock */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cronjobs_fire_job_lock(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cronjobs_fire_job_lock");
    int result = cronjobs_fire_job_lock(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cronjobs_fire_job_lock");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cronjobs_fire_job_lock <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
