/* gate: port__hermes_cli_update_cmd_py___warn_pending_fleet_restart_on_startup — traced copy of _warn_pending_fleet_restart_on_startup (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _warn_pending_fleet_restart_on_startup(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _warn_pending_fleet_restart_on_startup <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_warn_pending_fleet_restart_on_startup");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _warn_pending_fleet_restart_on_startup(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_warn_pending_fleet_restart_on_startup");
    }
    return 0;
}
