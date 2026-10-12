/* gate: real__utils_py___rewrite_in_place — GAP sentinel for _rewrite_in_place */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_rewrite_in_place";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _rewrite_in_place <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_rewrite_in_place");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_rewrite_in_place");
    }
    return 0;
}
