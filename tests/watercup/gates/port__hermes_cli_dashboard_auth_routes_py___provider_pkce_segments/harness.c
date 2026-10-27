/* gate: port__hermes_cli_dashboard_auth_routes_py___provider_pkce_segments — traced copy of _provider_pkce_segments (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void _provider_pkce_segments(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _provider_pkce_segments <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_provider_pkce_segments");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _provider_pkce_segments();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_provider_pkce_segments");
    }
    return 0;
}
