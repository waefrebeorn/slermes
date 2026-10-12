/* gate: real__gateway_run_py___abort_startup_if_shutdown_requested — GAP sentinel for _abort_startup_if_shutdown_requested */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_abort_startup_if_shutdown_requested";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _abort_startup_if_shutdown_requested <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_abort_startup_if_shutdown_requested");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_abort_startup_if_shutdown_requested");
    }
    return 0;
}
