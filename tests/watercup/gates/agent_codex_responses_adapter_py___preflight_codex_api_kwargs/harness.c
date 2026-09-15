/* gate: agent_codex_responses_adapter_py___preflight_codex_api_kwargs — traced copy of crx_preflight_codex_api_kwargs */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int crx_preflight_codex_api_kwargs(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("crx_preflight_codex_api_kwargs");
    int result = crx_preflight_codex_api_kwargs(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("crx_preflight_codex_api_kwargs");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s crx_preflight_codex_api_kwargs <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
