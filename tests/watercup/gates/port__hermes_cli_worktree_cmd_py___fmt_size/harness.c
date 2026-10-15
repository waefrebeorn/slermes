/* gate: port__hermes_cli_worktree_cmd_py___fmt_size — traced copy of worktree_cmd_fmt_size (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * worktree_cmd_fmt_size(int has_size, int size_mb);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s worktree_cmd_fmt_size <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("worktree_cmd_fmt_size");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = worktree_cmd_fmt_size(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("worktree_cmd_fmt_size");
    }
    return 0;
}
