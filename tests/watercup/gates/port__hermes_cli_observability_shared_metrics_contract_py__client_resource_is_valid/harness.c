/* gate: port__hermes_cli_observability_shared_metrics_contract_py__client_resource_is_valid — traced copy of smc_client_resource_is_valid (bool_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern bool smc_client_resource_is_valid(const char *resource_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s smc_client_resource_is_valid <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("smc_client_resource_is_valid");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = smc_client_resource_is_valid(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("smc_client_resource_is_valid");
    }
    return 0;
}
