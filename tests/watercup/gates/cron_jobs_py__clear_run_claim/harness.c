/* gate: cron_jobs_py__clear_run_claim — traced copy of cronjobs_clear_run_claim */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool cronjobs_clear_run_claim(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cronjobs_clear_run_claim");
    bool result = cronjobs_clear_run_claim(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cronjobs_clear_run_claim");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cronjobs_clear_run_claim <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
