/* gate: port__hermes_cli_update_cmd_py___restart_macos_launchd_gateways — traced copy of _restart_macos_launchd_gateways (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void _restart_macos_launchd_gateways(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _restart_macos_launchd_gateways <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_restart_macos_launchd_gateways");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _restart_macos_launchd_gateways();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_restart_macos_launchd_gateways");
    }
    return 0;
}
