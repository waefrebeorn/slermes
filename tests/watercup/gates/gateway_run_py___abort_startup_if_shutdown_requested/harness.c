/* gate: gateway_run_py___abort_startup_if_shutdown_requested — traced copy of grun_u_abort_startup_if_shutdown_requested */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int grun_u_abort_startup_if_shutdown_requested(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("grun_u_abort_startup_if_shutdown_requested");
    int result = grun_u_abort_startup_if_shutdown_requested(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("grun_u_abort_startup_if_shutdown_requested");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s grun_u_abort_startup_if_shutdown_requested <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
