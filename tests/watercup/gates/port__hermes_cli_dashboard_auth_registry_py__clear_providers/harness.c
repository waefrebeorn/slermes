/* gate: port__hermes_cli_dashboard_auth_registry_py__clear_providers — traced copy of dash_auth_clear_providers (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void dash_auth_clear_providers(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s dash_auth_clear_providers <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("dash_auth_clear_providers");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    dash_auth_clear_providers(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("dash_auth_clear_providers");
    }
    return 0;
}
