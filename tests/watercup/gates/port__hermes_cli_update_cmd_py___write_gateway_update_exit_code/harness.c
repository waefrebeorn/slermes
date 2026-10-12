/* gate: port__hermes_cli_update_cmd_py___write_gateway_update_exit_code — traced copy of upd_write_gateway_update_exit_code (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void upd_write_gateway_update_exit_code(bool ok);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s upd_write_gateway_update_exit_code <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("upd_write_gateway_update_exit_code");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    upd_write_gateway_update_exit_code(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("upd_write_gateway_update_exit_code");
    }
    return 0;
}
