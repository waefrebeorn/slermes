/* gate: port__hermes_cli_update_cmd_py___drain_or_signal_gateway_for_update — traced copy of _drain_or_signal_gateway_for_update (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _drain_or_signal_gateway_for_update(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _drain_or_signal_gateway_for_update <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_drain_or_signal_gateway_for_update");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _drain_or_signal_gateway_for_update(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_drain_or_signal_gateway_for_update");
    }
    return 0;
}
