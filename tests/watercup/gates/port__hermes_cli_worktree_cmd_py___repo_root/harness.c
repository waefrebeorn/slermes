/* gate: port__hermes_cli_worktree_cmd_py___repo_root — traced copy of worktree_cmd_repo_root (charptr_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * worktree_cmd_repo_root(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s worktree_cmd_repo_root <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("worktree_cmd_repo_root");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = worktree_cmd_repo_root();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[64];
    snprintf(buf, sizeof(buf), "%p", (void*)&result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("worktree_cmd_repo_root");
    }
    return 0;
}
