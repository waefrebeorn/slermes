/* gate: real__gateway_restart_loop_guard_py___chain_ending_at — GAP sentinel for _chain_ending_at */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_chain_ending_at";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _chain_ending_at <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_chain_ending_at");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_chain_ending_at");
    }
    return 0;
}
