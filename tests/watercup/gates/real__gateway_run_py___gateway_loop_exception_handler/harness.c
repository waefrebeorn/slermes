/* gate: real__gateway_run_py___gateway_loop_exception_handler — GAP sentinel for _gateway_loop_exception_handler */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_gateway_loop_exception_handler";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _gateway_loop_exception_handler <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_gateway_loop_exception_handler");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_gateway_loop_exception_handler");
    }
    return 0;
}
