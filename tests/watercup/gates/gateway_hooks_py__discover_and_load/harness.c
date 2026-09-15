/* gate: gateway_hooks_py__discover_and_load — traced copy of cli_gateway_hooks_discover_and_load */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cli_gateway_hooks_discover_and_load(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cli_gateway_hooks_discover_and_load");
    int result = cli_gateway_hooks_discover_and_load(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cli_gateway_hooks_discover_and_load");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_gateway_hooks_discover_and_load <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
