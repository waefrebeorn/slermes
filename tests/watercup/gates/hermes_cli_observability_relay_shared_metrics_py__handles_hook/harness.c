/* gate: hermes_cli_observability_relay_shared_metrics_py__handles_hook — traced copy of rsm_handles_hook */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int rsm_handles_hook(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("rsm_handles_hook");
    int result = rsm_handles_hook(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("rsm_handles_hook");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s rsm_handles_hook <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
