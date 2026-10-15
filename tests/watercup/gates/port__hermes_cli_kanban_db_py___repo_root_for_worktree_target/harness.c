/* gate: port__hermes_cli_kanban_db_py___repo_root_for_worktree_target — traced copy of _repo_root_for_worktree_target (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char* _repo_root_for_worktree_target(const char* path);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _repo_root_for_worktree_target <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_repo_root_for_worktree_target");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char* result = _repo_root_for_worktree_target(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_repo_root_for_worktree_target");
    }
    return 0;
}
