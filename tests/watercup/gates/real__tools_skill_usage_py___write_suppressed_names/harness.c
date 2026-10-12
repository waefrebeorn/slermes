/* gate: real__tools_skill_usage_py___write_suppressed_names — GAP sentinel for _write_suppressed_names */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_write_suppressed_names";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _write_suppressed_names <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_write_suppressed_names");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_write_suppressed_names");
    }
    return 0;
}
