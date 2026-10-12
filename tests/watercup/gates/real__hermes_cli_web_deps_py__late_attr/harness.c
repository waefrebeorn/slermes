/* gate: real__hermes_cli_web_deps_py__late_attr — GAP sentinel for late_attr */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "late_attr";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s late_attr <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("late_attr");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("late_attr");
    }
    return 0;
}
