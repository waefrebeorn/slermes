/* gate: agent_secret_sources_registry_py__applied_any — traced copy of ssr_applied_any */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool ssr_applied_any(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("ssr_applied_any");
    bool result = ssr_applied_any(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ssr_applied_any");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ssr_applied_any <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
