/* gate: port__gateway_run_py___terminal_scope_cwd — traced copy of _terminal_scope_cwd (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * _terminal_scope_cwd(const char *default_val);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _terminal_scope_cwd <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_terminal_scope_cwd");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = _terminal_scope_cwd(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_terminal_scope_cwd");
    }
    return 0;
}
