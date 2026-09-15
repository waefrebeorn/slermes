/* gate: agent_anthropic_adapter_py___resolve_positive_anthropic_max_tokens — traced copy of anthropic_resolve_positive_max_tokens */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int anthropic_resolve_positive_max_tokens(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("anthropic_resolve_positive_max_tokens");
    int result = anthropic_resolve_positive_max_tokens(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("anthropic_resolve_positive_max_tokens");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s anthropic_resolve_positive_max_tokens <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
