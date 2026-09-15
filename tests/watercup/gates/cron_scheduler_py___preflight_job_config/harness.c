/* gate: cron_scheduler_py___preflight_job_config — traced copy of sched_preflight_job_config */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int sched_preflight_job_config(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("sched_preflight_job_config");
    int result = sched_preflight_job_config(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sched_preflight_job_config");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sched_preflight_job_config <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
