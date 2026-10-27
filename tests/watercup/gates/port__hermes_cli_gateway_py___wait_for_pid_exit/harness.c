/* gate: port__hermes_cli_gateway_py___wait_for_pid_exit — traced copy of gw_wait_for_pid_exit (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern bool gw_wait_for_pid_exit(pid_t pid, double timeout_s);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gw_wait_for_pid_exit <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("gw_wait_for_pid_exit");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = gw_wait_for_pid_exit((int)strtol(argv[i], NULL, 10), strtod(argv[2 + (i - 2 + 1) % 5], NULL));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("gw_wait_for_pid_exit");
    }
    return 0;
}
