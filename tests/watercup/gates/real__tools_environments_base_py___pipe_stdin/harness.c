/* gate: real__tools_environments_base_py___pipe_stdin — GAP sentinel for _pipe_stdin */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_pipe_stdin";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _pipe_stdin <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_pipe_stdin");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_pipe_stdin");
    }
    return 0;
}
