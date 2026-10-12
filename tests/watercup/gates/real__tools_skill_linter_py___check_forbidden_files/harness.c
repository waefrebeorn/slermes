/* gate: real__tools_skill_linter_py___check_forbidden_files — GAP sentinel for _check_forbidden_files */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_check_forbidden_files";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _check_forbidden_files <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_check_forbidden_files");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_check_forbidden_files");
    }
    return 0;
}
