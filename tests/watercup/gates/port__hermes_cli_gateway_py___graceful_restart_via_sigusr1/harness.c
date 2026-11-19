/* gate: port__hermes_cli_gateway_py___graceful_restart_via_sigusr1 — traced copy of graceful_restart_via_sigusr1 (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern bool graceful_restart_via_sigusr1(int pid, double drain_timeout);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s graceful_restart_via_sigusr1 <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("graceful_restart_via_sigusr1");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = graceful_restart_via_sigusr1((int)strtol(argv[i], NULL, 10), strtod(argv[2 + (i - 2 + 1) % 5], NULL));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("graceful_restart_via_sigusr1");
    }
    return 0;
}
