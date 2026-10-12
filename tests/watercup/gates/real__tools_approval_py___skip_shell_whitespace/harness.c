/* gate: real__tools_approval_py___skip_shell_whitespace — GAP sentinel for _skip_shell_whitespace */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_skip_shell_whitespace";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _skip_shell_whitespace <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_skip_shell_whitespace");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_skip_shell_whitespace");
    }
    return 0;
}
