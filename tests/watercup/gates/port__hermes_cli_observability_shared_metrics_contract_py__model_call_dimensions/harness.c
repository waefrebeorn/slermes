/* gate: port__hermes_cli_observability_shared_metrics_contract_py__model_call_dimensions — traced copy of smc_model_call_dimensions (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * smc_model_call_dimensions(const char *event_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s smc_model_call_dimensions <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("smc_model_call_dimensions");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = smc_model_call_dimensions(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("smc_model_call_dimensions");
    }
    return 0;
}
