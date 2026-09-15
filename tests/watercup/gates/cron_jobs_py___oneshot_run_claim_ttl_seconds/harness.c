/* gate: cron_jobs_py___oneshot_run_claim_ttl_seconds — traced copy of cron_jobs_u_oneshot_run_claim_ttl_seconds */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cron_jobs_u_oneshot_run_claim_ttl_seconds(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cron_jobs_u_oneshot_run_claim_ttl_seconds");
    int result = cron_jobs_u_oneshot_run_claim_ttl_seconds(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cron_jobs_u_oneshot_run_claim_ttl_seconds");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cron_jobs_u_oneshot_run_claim_ttl_seconds <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
