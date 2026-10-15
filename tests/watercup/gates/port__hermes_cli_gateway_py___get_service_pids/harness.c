/* gate: port__hermes_cli_gateway_py___get_service_pids — traced copy of get_service_pids (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int get_service_pids(int *pids, int max);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s get_service_pids <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("get_service_pids");
    int _o0 = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = get_service_pids(&_o0, (int)strtol(argv[2 + (i - 2 + 1) % 2], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%d", (int)_o0); WCUP_VALSTR(_b); }
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("get_service_pids");
    }
    return 0;
}
