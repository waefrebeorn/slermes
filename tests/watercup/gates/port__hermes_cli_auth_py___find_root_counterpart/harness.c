/* gate: port__hermes_cli_auth_py___find_root_counterpart — traced copy of _find_root_counterpart (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _find_root_counterpart(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _find_root_counterpart <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_find_root_counterpart");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _find_root_counterpart(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_find_root_counterpart");
    }
    return 0;
}
