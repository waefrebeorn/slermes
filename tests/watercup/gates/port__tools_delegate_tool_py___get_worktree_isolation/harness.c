/* gate: port__tools_delegate_tool_py___get_worktree_isolation — traced copy of dlg_get_worktree_isolation (int_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int dlg_get_worktree_isolation(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s dlg_get_worktree_isolation <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("dlg_get_worktree_isolation");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = dlg_get_worktree_isolation();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("dlg_get_worktree_isolation");
    }
    return 0;
}
