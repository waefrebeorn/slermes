/* gate: port__hermes_cli_gateway_py___escalate_wedged_gateway — traced copy of gw_escalate_wedged_gateway (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void gw_escalate_wedged_gateway(pid_t pid, double grace_s);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gw_escalate_wedged_gateway <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("gw_escalate_wedged_gateway");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    gw_escalate_wedged_gateway(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("gw_escalate_wedged_gateway");
    }
    return 0;
}
