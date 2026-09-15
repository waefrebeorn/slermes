/* gate: hermes_cli_observability_shared_metrics_py___pending_period_count — traced copy of sm_pending_period_count */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int sm_pending_period_count(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("sm_pending_period_count");
    int result = sm_pending_period_count(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sm_pending_period_count");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sm_pending_period_count <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
