/* gate: agent_shared_metrics_contract_py__is_valid_execution_surface — traced copy of smc_is_valid_execution_surface */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool smc_is_valid_execution_surface(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("smc_is_valid_execution_surface");
    bool result = smc_is_valid_execution_surface(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("smc_is_valid_execution_surface");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s smc_is_valid_execution_surface <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
