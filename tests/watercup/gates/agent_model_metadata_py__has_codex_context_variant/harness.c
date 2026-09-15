/* gate: agent_model_metadata_py__has_codex_context_variant — traced copy of has_codex_context_variant */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool has_codex_context_variant(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("has_codex_context_variant");
    bool result = has_codex_context_variant(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("has_codex_context_variant");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s has_codex_context_variant <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
