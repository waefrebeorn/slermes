/* gate: real__tools_working_diff_py__collect_working_diff — GAP sentinel for collect_working_diff */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "collect_working_diff";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s collect_working_diff <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("collect_working_diff");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("collect_working_diff");
    }
    return 0;
}
