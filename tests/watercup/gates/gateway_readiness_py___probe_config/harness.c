/* gate: gateway_readiness_py___probe_config — traced copy of gateway_readiness_u_probe_config */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gateway_readiness_u_probe_config(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gateway_readiness_u_probe_config");
    int result = gateway_readiness_u_probe_config(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gateway_readiness_u_probe_config");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gateway_readiness_u_probe_config <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
