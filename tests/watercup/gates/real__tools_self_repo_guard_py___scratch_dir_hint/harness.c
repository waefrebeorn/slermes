/* gate: real__tools_self_repo_guard_py___scratch_dir_hint — GAP sentinel for _scratch_dir_hint */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_scratch_dir_hint";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _scratch_dir_hint <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_scratch_dir_hint");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_scratch_dir_hint");
    }
    return 0;
}
