/* gate: real__tools_checkpoint_manager_py__session_diff — GAP sentinel for session_diff */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "session_diff";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s session_diff <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("session_diff");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("session_diff");
    }
    return 0;
}
