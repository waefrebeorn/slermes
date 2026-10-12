/* gate: real__hermes_state_py___in_test_context — GAP sentinel for _in_test_context */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_in_test_context";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _in_test_context <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_in_test_context");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_in_test_context");
    }
    return 0;
}
