/* gate: real__gateway_dead_targets_py___flush_locked — GAP sentinel for _flush_locked */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_flush_locked";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _flush_locked <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_flush_locked");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_flush_locked");
    }
    return 0;
}
