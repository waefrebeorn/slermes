/* gate: port__tools_fuzzy_match_py___strategy_exact — GAP sentinel for _strategy_exact */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_strategy_exact";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _strategy_exact <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_strategy_exact");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_strategy_exact");
    }
    return 0;
}
