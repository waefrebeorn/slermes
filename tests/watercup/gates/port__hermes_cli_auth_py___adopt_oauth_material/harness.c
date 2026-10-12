/* gate: port__hermes_cli_auth_py___adopt_oauth_material — traced copy of _adopt_oauth_material (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _adopt_oauth_material(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _adopt_oauth_material <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_adopt_oauth_material");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _adopt_oauth_material(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_adopt_oauth_material");
    }
    return 0;
}
