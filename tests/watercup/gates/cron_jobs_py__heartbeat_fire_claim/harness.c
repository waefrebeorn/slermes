/* gate: cron_jobs_py__heartbeat_fire_claim — traced copy of cronjobs_heartbeat_fire_claim */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cronjobs_heartbeat_fire_claim(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cronjobs_heartbeat_fire_claim");
    int result = cronjobs_heartbeat_fire_claim(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cronjobs_heartbeat_fire_claim");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cronjobs_heartbeat_fire_claim <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
