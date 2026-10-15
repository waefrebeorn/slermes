/* gate: port__cli_py___maybe_remap_for_light_mode — traced copy of _maybe_remap_for_light_mode (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * _maybe_remap_for_light_mode(const char *hex_color);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _maybe_remap_for_light_mode <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_maybe_remap_for_light_mode");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = _maybe_remap_for_light_mode(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_maybe_remap_for_light_mode");
    }
    return 0;
}
