/* gate: real__agent_rate_limit_tracker_py___safe_int — GAP sentinel for _safe_int */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_safe_int";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _safe_int <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_safe_int");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_safe_int");
    }
    return 0;
}
