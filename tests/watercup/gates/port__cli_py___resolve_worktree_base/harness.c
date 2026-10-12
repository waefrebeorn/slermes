/* gate: port__cli_py___resolve_worktree_base — traced copy of _resolve_worktree_base (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _resolve_worktree_base(const char *repo_root, double fetch_timeout,
                            double freshness_window,
                            char **out_base_ref, char **out_base_label);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _resolve_worktree_base <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_resolve_worktree_base");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _resolve_worktree_base(argv[i], argv[2 + (i - 2 + 1) % 5], argv[2 + (i - 2 + 2) % 5], argv[2 + (i - 2 + 3) % 5], argv[2 + (i - 2 + 4) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_resolve_worktree_base");
    }
    return 0;
}
