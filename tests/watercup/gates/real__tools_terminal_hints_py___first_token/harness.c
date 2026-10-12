/* gate: real__tools_terminal_hints_py___first_token — GAP sentinel for _first_token */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_first_token";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _first_token <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_first_token");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_first_token");
    }
    return 0;
}
