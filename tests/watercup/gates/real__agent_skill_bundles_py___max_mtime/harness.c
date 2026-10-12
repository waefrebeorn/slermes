/* gate: real__agent_skill_bundles_py___max_mtime — GAP sentinel for _max_mtime */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_max_mtime";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _max_mtime <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_max_mtime");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_max_mtime");
    }
    return 0;
}
