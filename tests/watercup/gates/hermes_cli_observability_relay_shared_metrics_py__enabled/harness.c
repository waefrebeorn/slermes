/* gate: hermes_cli_observability_relay_shared_metrics_py__enabled — traced copy of rsm_enabled */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool rsm_enabled(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("rsm_enabled");
    bool result = rsm_enabled(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("rsm_enabled");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s rsm_enabled <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
