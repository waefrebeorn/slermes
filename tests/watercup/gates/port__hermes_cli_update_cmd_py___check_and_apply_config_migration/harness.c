/* gate: port__hermes_cli_update_cmd_py___check_and_apply_config_migration — traced copy of _check_and_apply_config_migration (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _check_and_apply_config_migration(int assume_yes, int gateway_mode, const char *pre_update_snapshot_id);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _check_and_apply_config_migration <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_check_and_apply_config_migration");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _check_and_apply_config_migration(argv[i], argv[2 + (i - 2 + 1) % 5], argv[2 + (i - 2 + 2) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_check_and_apply_config_migration");
    }
    return 0;
}
