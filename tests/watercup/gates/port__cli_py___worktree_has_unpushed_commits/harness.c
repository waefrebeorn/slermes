/* gate: port__cli_py___worktree_has_unpushed_commits — traced copy of cli_worktree_has_unpushed_commits (int_other) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int cli_worktree_has_unpushed_commits(const char *worktree_path, int timeout);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_worktree_has_unpushed_commits <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_worktree_has_unpushed_commits");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = cli_worktree_has_unpushed_commits(argv[i], (int)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_worktree_has_unpushed_commits");
    }
    return 0;
}
