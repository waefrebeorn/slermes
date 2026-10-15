/* gate: port__gateway_platforms_signal_rate_limit_py___refill — traced copy of cli_gateway_platforms_signal_rate_limit__refill (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int cli_gateway_platforms_signal_rate_limit__refill(int *bucket, int capacity, int tokens_per_second);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_gateway_platforms_signal_rate_limit__refill <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_gateway_platforms_signal_rate_limit__refill");
    int _o0 = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = cli_gateway_platforms_signal_rate_limit__refill(&_o0, (int)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%d", (int)_o0); WCUP_VALSTR(_b); }
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_gateway_platforms_signal_rate_limit__refill");
    }
    return 0;
}
