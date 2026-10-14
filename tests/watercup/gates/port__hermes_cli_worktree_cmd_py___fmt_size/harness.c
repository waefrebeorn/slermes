/* gate: port__hermes_cli_worktree_cmd_py___fmt_size — traced copy of worktree_cmd_fmt_size (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void worktree_cmd_fmt_size(char **actions, size_t n);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s worktree_cmd_fmt_size <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("worktree_cmd_fmt_size");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    worktree_cmd_fmt_size(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("worktree_cmd_fmt_size");
    }
    return 0;
}
