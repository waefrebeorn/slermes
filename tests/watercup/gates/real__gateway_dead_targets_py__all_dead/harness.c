/* gate: real__gateway_dead_targets_py__all_dead — GAP sentinel for all_dead */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "all_dead";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s all_dead <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("all_dead");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("all_dead");
    }
    return 0;
}
