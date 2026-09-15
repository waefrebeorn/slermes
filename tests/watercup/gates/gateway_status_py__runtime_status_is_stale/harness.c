/* gate: gateway_status_py__runtime_status_is_stale — traced copy of gstat_runtime_status_is_stale */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gstat_runtime_status_is_stale(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gstat_runtime_status_is_stale");
    int result = gstat_runtime_status_is_stale(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gstat_runtime_status_is_stale");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gstat_runtime_status_is_stale <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
