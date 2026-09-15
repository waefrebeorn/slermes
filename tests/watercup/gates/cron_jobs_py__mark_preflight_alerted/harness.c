/* gate: cron_jobs_py__mark_preflight_alerted — traced copy of cronjobs_mark_preflight_alerted */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool cronjobs_mark_preflight_alerted(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cronjobs_mark_preflight_alerted");
    bool result = cronjobs_mark_preflight_alerted(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cronjobs_mark_preflight_alerted");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cronjobs_mark_preflight_alerted <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
