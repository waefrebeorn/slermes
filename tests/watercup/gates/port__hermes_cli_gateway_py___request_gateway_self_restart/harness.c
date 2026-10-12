/* gate: port__hermes_cli_gateway_py___request_gateway_self_restart — traced copy of request_gateway_self_restart (bool_int) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern bool request_gateway_self_restart(int pid);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s request_gateway_self_restart <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("request_gateway_self_restart");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = request_gateway_self_restart((int)strtol(argv[i], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("request_gateway_self_restart");
    }
    return 0;
}
