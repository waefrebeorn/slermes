/* gate: real__agent_relay_runtime_py__reset_for_tests — GAP sentinel for reset_for_tests */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "reset_for_tests";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s reset_for_tests <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("reset_for_tests");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("reset_for_tests");
    }
    return 0;
}
