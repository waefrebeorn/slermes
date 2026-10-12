/* gate: port__hermes_cli_update_cmd_py___park_stashed_changes — traced copy of _park_stashed_changes (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _park_stashed_changes(const char *stash_ref);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _park_stashed_changes <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_park_stashed_changes");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _park_stashed_changes(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_park_stashed_changes");
    }
    return 0;
}
