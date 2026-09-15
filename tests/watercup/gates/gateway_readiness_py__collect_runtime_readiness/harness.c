/* gate: gateway_readiness_py__collect_runtime_readiness — traced copy of gateway_readiness_collect_runtime_readiness */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gateway_readiness_collect_runtime_readiness(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gateway_readiness_collect_runtime_readiness");
    int result = gateway_readiness_collect_runtime_readiness(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gateway_readiness_collect_runtime_readiness");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gateway_readiness_collect_runtime_readiness <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
