/* gate: hermes_cli_kanban_db_py___is_linked_worktree_checkout — traced copy of kdb_is_linked_worktree_checkout */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool kdb_is_linked_worktree_checkout(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("kdb_is_linked_worktree_checkout");
    bool result = kdb_is_linked_worktree_checkout(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("kdb_is_linked_worktree_checkout");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s kdb_is_linked_worktree_checkout <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
