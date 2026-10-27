/* gate: port__hermes_cli_setup_py___blank_slate_minimal_toolsets — traced copy of _blank_slate_minimal_toolsets (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void _blank_slate_minimal_toolsets(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _blank_slate_minimal_toolsets <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_blank_slate_minimal_toolsets");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _blank_slate_minimal_toolsets();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_blank_slate_minimal_toolsets");
    }
    return 0;
}
