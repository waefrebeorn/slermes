/* gate: port__hermes_cli_worktree_cmd_py__cmd_worktree — traced copy of worktree_cmd_cmd_worktree (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int worktree_cmd_cmd_worktree(const char *repo, const char *action,
                              int dry_run, int trees_only, int branches_only);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s worktree_cmd_cmd_worktree <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("worktree_cmd_cmd_worktree");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = worktree_cmd_cmd_worktree(argv[i], argv[2 + (i - 2 + 1) % 5], (int)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 3) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 4) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("worktree_cmd_cmd_worktree");
    }
    return 0;
}
