/* gate: port__hermes_cli_update_inventory_py___serve_unit_matches_profile — traced copy of _serve_unit_matches_profile (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _serve_unit_matches_profile(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _serve_unit_matches_profile <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_serve_unit_matches_profile");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _serve_unit_matches_profile(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_serve_unit_matches_profile");
    }
    return 0;
}
