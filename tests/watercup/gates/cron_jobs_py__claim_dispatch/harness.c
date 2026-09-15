/* gate: cron_jobs_py__claim_dispatch — traced copy of cronjobs_claim_dispatch */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool cronjobs_claim_dispatch(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cronjobs_claim_dispatch");
    bool result = cronjobs_claim_dispatch(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cronjobs_claim_dispatch");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cronjobs_claim_dispatch <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
