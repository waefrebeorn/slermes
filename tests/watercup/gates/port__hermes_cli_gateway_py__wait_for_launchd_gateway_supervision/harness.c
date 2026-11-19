/* gate: port__hermes_cli_gateway_py__wait_for_launchd_gateway_supervision — traced copy of wait_for_launchd_gateway_supervision (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern bool wait_for_launchd_gateway_supervision(const char *label, double timeout,
                                          double poll_interval);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wait_for_launchd_gateway_supervision <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("wait_for_launchd_gateway_supervision");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = wait_for_launchd_gateway_supervision(argv[i], strtod(argv[2 + (i - 2 + 1) % 5], NULL), strtod(argv[2 + (i - 2 + 2) % 5], NULL));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("wait_for_launchd_gateway_supervision");
    }
    return 0;
}
