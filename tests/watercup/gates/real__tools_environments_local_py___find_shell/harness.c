/* gate: real__tools_environments_local_py___find_shell — GAP sentinel for _find_shell */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_find_shell";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _find_shell <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_find_shell");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_find_shell");
    }
    return 0;
}
