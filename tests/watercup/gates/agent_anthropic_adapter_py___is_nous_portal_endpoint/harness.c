/* gate: agent_anthropic_adapter_py___is_nous_portal_endpoint — traced copy of anthropic_adapter_is_nous_portal_endpoint */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool anthropic_adapter_is_nous_portal_endpoint(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("anthropic_adapter_is_nous_portal_endpoint");
    bool result = anthropic_adapter_is_nous_portal_endpoint(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("anthropic_adapter_is_nous_portal_endpoint");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s anthropic_adapter_is_nous_portal_endpoint <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
