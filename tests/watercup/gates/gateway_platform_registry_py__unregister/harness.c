/* gate: gateway_platform_registry_py__unregister — traced copy of cli_gateway_platform_registry_unregister */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cli_gateway_platform_registry_unregister(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cli_gateway_platform_registry_unregister");
    int result = cli_gateway_platform_registry_unregister(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cli_gateway_platform_registry_unregister");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_gateway_platform_registry_unregister <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
