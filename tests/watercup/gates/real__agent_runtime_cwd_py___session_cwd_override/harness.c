/* gate: real__agent_runtime_cwd_py___session_cwd_override — GAP sentinel for _session_cwd_override */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_session_cwd_override";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _session_cwd_override <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_session_cwd_override");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_session_cwd_override");
    }
    return 0;
}
