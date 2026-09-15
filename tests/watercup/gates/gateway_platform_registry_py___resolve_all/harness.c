/* gate: gateway_platform_registry_py___resolve_all — traced copy of gateway_platform_registry_u_resolve_all */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gateway_platform_registry_u_resolve_all(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gateway_platform_registry_u_resolve_all");
    int result = gateway_platform_registry_u_resolve_all(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gateway_platform_registry_u_resolve_all");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gateway_platform_registry_u_resolve_all <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
