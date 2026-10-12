/* gate: port__hermes_cli_managed_uv_py___macos_sign_managed_python — traced copy of _macos_sign_managed_python (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _macos_sign_managed_python(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _macos_sign_managed_python <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_macos_sign_managed_python");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _macos_sign_managed_python(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_macos_sign_managed_python");
    }
    return 0;
}
