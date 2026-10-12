/* gate: port__hermes_cli_update_cmd_py___surviving_gateway_pids_after_failed_restart — traced copy of _surviving_gateway_pids_after_failed_restart (int_str_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int _surviving_gateway_pids_after_failed_restart(const char *service, const char *profile);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _surviving_gateway_pids_after_failed_restart <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_surviving_gateway_pids_after_failed_restart");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = _surviving_gateway_pids_after_failed_restart(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_surviving_gateway_pids_after_failed_restart");
    }
    return 0;
}
