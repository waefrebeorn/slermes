/* gate: real__agent_turn_liveness_py___abort_message — GAP sentinel for _abort_message */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_abort_message";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _abort_message <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_abort_message");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_abort_message");
    }
    return 0;
}
