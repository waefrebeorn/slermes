/* gate: real__gateway_run_py__should_exit_cleanly — GAP sentinel for should_exit_cleanly */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "should_exit_cleanly";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s should_exit_cleanly <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("should_exit_cleanly");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("should_exit_cleanly");
    }
    return 0;
}
