/* gate: real__tools_environments_local_py___prepend_git_bash_dirs — GAP sentinel for _prepend_git_bash_dirs */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_prepend_git_bash_dirs";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _prepend_git_bash_dirs <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_prepend_git_bash_dirs");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_prepend_git_bash_dirs");
    }
    return 0;
}
