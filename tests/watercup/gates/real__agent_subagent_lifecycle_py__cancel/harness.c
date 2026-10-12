/* gate: real__agent_subagent_lifecycle_py__cancel — GAP sentinel for cancel */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "cancel";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cancel <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cancel");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cancel");
    }
    return 0;
}
