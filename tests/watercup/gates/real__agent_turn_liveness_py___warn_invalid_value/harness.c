/* gate: real__agent_turn_liveness_py___warn_invalid_value — GAP sentinel for _warn_invalid_value */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_warn_invalid_value";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _warn_invalid_value <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_warn_invalid_value");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_warn_invalid_value");
    }
    return 0;
}
