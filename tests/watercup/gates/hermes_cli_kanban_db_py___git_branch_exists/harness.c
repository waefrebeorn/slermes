/* gate: hermes_cli_kanban_db_py___git_branch_exists — traced copy of kdb_git_branch_exists */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int kdb_git_branch_exists(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("kdb_git_branch_exists");
    int result = kdb_git_branch_exists(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("kdb_git_branch_exists");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s kdb_git_branch_exists <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
