/* gate: tools_self_repo_guard_py___mutates_worktree — traced copy of srg_mutates_worktree */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int srg_mutates_worktree(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("srg_mutates_worktree");
    int result = srg_mutates_worktree(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("srg_mutates_worktree");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s srg_mutates_worktree <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
