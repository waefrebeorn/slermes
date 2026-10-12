/* gate: port__hermes_cli_update_cmd_py___portable_git_candidates — traced copy of _portable_git_candidates (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _portable_git_candidates(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _portable_git_candidates <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_portable_git_candidates");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _portable_git_candidates(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_portable_git_candidates");
    }
    return 0;
}
