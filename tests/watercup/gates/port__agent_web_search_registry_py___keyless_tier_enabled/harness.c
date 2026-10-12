/* gate: port__agent_web_search_registry_py___keyless_tier_enabled — traced copy of _keyless_tier_enabled (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _keyless_tier_enabled(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _keyless_tier_enabled <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_keyless_tier_enabled");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _keyless_tier_enabled(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_keyless_tier_enabled");
    }
    return 0;
}
