/* gate: cron_scheduler_py__try_register_running_job — traced copy of sched_try_register_running_job */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int sched_try_register_running_job(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("sched_try_register_running_job");
    int result = sched_try_register_running_job(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sched_try_register_running_job");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sched_try_register_running_job <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
