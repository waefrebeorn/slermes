/* gate: port__hermes_cli_plugin_capabilities_py___log_capability_decision — traced copy of _log_capability_decision (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void _log_capability_decision(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _log_capability_decision <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_log_capability_decision");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _log_capability_decision();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_log_capability_decision");
    }
    return 0;
}
