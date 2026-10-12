/* gate: real__tools_self_repo_guard_py___command_parts — GAP sentinel for _command_parts */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_command_parts";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _command_parts <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_command_parts");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_command_parts");
    }
    return 0;
}
