/* gate: port__hermes_cli_update_cmd_py___defer_update_for_self_lock — traced copy of _defer_update_for_self_lock (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _defer_update_for_self_lock(const char *loaded);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _defer_update_for_self_lock <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_defer_update_for_self_lock");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _defer_update_for_self_lock(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_defer_update_for_self_lock");
    }
    return 0;
}
