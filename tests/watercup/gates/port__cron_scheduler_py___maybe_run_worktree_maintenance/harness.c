/* gate: port__cron_scheduler_py___maybe_run_worktree_maintenance — traced copy of _maybe_run_worktree_maintenance (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _maybe_run_worktree_maintenance(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _maybe_run_worktree_maintenance <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_maybe_run_worktree_maintenance");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _maybe_run_worktree_maintenance(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_maybe_run_worktree_maintenance");
    }
    return 0;
}
