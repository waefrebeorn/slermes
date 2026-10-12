/* gate: port__hermes_cli_update_cmd_py___verify_and_restore_one_state_db — traced copy of _verify_and_restore_one_state_db (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _verify_and_restore_one_state_db(const char *home, const char *label);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _verify_and_restore_one_state_db <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_verify_and_restore_one_state_db");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _verify_and_restore_one_state_db(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_verify_and_restore_one_state_db");
    }
    return 0;
}
