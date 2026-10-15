/* gate: port__hermes_cli_worktree_gc_py__worktrees_summary — traced copy of wgc_worktrees_summary (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void wgc_worktrees_summary(const char *repo_root, size_t *out_count,
                           int *out_size_mb, int *has_size);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wgc_worktrees_summary <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("wgc_worktrees_summary");
    size_t _o1 = 0;
    int _o2 = 0;
    int _o3 = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    wgc_worktrees_summary(argv[i], &_o1, &_o2, &_o3);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%zu", (size_t)_o1); WCUP_VALSTR(_b); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%d", (int)_o2); WCUP_VALSTR(_b); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%d", (int)_o3); WCUP_VALSTR(_b); }
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("wgc_worktrees_summary");
    }
    return 0;
}
