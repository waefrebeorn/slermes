/* gate: port__tools_browser_camofox_py___camofox_identity_override — traced copy of _camofox_identity_override (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern bool _camofox_identity_override(const char *flag_name, bool default_val);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _camofox_identity_override <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_camofox_identity_override");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = _camofox_identity_override(argv[i], (int)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_camofox_identity_override");
    }
    return 0;
}
