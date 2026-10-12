/* gate: port__hermes_cli_web_routers_profiles_py___sidebar_profile_cache_clear — traced copy of _sidebar_profile_cache_clear (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _sidebar_profile_cache_clear(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _sidebar_profile_cache_clear <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_sidebar_profile_cache_clear");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _sidebar_profile_cache_clear(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_sidebar_profile_cache_clear");
    }
    return 0;
}
