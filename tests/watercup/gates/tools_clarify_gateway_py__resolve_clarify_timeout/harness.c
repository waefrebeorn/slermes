/* gate: tools_clarify_gateway_py__resolve_clarify_timeout — traced copy of tools_clarify_gateway_resolve_clarify_timeout */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_clarify_gateway_resolve_clarify_timeout(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_clarify_gateway_resolve_clarify_timeout");
    int result = tools_clarify_gateway_resolve_clarify_timeout(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_clarify_gateway_resolve_clarify_timeout");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_clarify_gateway_resolve_clarify_timeout <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
