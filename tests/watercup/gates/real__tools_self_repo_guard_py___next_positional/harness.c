/* gate: real__tools_self_repo_guard_py___next_positional — GAP sentinel for _next_positional */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_next_positional";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _next_positional <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_next_positional");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_next_positional");
    }
    return 0;
}
