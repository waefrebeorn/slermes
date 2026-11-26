/* gate: cron_jobs_py___heartbeat_fire_claim_locked — traced copy of cronjobs_heartbeat_fire_claim_locked */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cronjobs_heartbeat_fire_claim_locked(const char *a0, const char *a1);

static void trace_one(const char **in, int n, int idx) {
    WCUP_BEGIN("cronjobs_heartbeat_fire_claim_locked");
    int result = cronjobs_heartbeat_fire_claim_locked(in[(idx + 0) % n], in[(idx + 1) % n]);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cronjobs_heartbeat_fire_claim_locked");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cronjobs_heartbeat_fire_claim_locked <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one((const char **)(argv + 2), argc - 2, i - 2);
    }
    return 0;
}
