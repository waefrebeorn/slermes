/* gate: hermes_cli_gateway_py__ensure_gateway_service — traced copy of ensure_gateway_service */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool ensure_gateway_service(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("ensure_gateway_service");
    bool result = ensure_gateway_service(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ensure_gateway_service");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ensure_gateway_service <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
