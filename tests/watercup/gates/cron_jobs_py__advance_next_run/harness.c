/* gate: cron_jobs_py__advance_next_run — traced copy of cronjobs_advance_next_run */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool cronjobs_advance_next_run(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cronjobs_advance_next_run");
    bool result = cronjobs_advance_next_run(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cronjobs_advance_next_run");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cronjobs_advance_next_run <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
