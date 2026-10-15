/* gate: port__hermes_cli_worktree_gc_py___tree_size_mb — traced copy of wgc_tree_size_mb (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int wgc_tree_size_mb(const char *path, int *has_size);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wgc_tree_size_mb <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("wgc_tree_size_mb");
    int _o1 = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = wgc_tree_size_mb(argv[i], &_o1);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%d", (int)_o1); WCUP_VALSTR(_b); }
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("wgc_tree_size_mb");
    }
    return 0;
}
