/* gate: cron_jobs_py__remove_job — traced copy of cronjobs_remove_job */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool cronjobs_remove_job(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cronjobs_remove_job");
    bool result = cronjobs_remove_job(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cronjobs_remove_job");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cronjobs_remove_job <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
