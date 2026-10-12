/* gate: port__hermes_cli_worktree_gc_py___tree_size_mb — GAP sentinel for _tree_size_mb */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_tree_size_mb";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _tree_size_mb <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_tree_size_mb");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_tree_size_mb");
    }
    return 0;
}
