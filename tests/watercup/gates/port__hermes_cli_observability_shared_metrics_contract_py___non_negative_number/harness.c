/* gate: port__hermes_cli_observability_shared_metrics_contract_py___non_negative_number — traced copy of smc_non_negative_number (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int smc_non_negative_number(int kind, long long ival, double dval, double *out);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s smc_non_negative_number <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("smc_non_negative_number");
    double _o3 = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = smc_non_negative_number((int)strtol(argv[i], NULL, 10), (long)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10), strtod(argv[2 + (i - 2 + 2) % 5], NULL), &_o3);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    { char _b[64]; snprintf(_b, sizeof(_b), "%.17g", (double)_o3); WCUP_VALSTR(_b); }
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("smc_non_negative_number");
    }
    return 0;
}
