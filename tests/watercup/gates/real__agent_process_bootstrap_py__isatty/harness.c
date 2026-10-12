/* gate: real__agent_process_bootstrap_py__isatty — GAP sentinel for isatty */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "isatty";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s isatty <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("isatty");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("isatty");
    }
    return 0;
}
