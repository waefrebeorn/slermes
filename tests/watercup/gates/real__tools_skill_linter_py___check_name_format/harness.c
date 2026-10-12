/* gate: real__tools_skill_linter_py___check_name_format — GAP sentinel for _check_name_format */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_check_name_format";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _check_name_format <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_check_name_format");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_check_name_format");
    }
    return 0;
}
