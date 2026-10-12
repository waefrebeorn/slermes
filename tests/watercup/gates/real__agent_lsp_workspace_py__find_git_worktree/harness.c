/* gate: real__agent_lsp_workspace_py__find_git_worktree — GAP sentinel for find_git_worktree */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "find_git_worktree";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s find_git_worktree <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("find_git_worktree");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("find_git_worktree");
    }
    return 0;
}
