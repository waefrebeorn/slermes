/* gate: port__hermes_cli_update_cmd_py___clear_fleet_restart_pending_marker — traced copy of _clear_fleet_restart_pending_marker (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void _clear_fleet_restart_pending_marker(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _clear_fleet_restart_pending_marker <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_clear_fleet_restart_pending_marker");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _clear_fleet_restart_pending_marker();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_clear_fleet_restart_pending_marker");
    }
    return 0;
}
