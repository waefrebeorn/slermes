/* gate: port__cli_py___prune_stale_worktrees — traced copy of _prune_stale_worktrees (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _prune_stale_worktrees(const char *repo_root, int max_age_hours);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _prune_stale_worktrees <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_prune_stale_worktrees");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _prune_stale_worktrees(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_prune_stale_worktrees");
    }
    return 0;
}
