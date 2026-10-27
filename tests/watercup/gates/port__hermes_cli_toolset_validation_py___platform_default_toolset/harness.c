/* gate: port__hermes_cli_toolset_validation_py___platform_default_toolset — traced copy of _platform_default_toolset (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void _platform_default_toolset(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _platform_default_toolset <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_platform_default_toolset");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _platform_default_toolset();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_platform_default_toolset");
    }
    return 0;
}
