/* gate: agent_model_metadata_py___resolve_codex_oauth_context_length — traced copy of resolve_codex_oauth_context_length */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int resolve_codex_oauth_context_length(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("resolve_codex_oauth_context_length");
    int result = resolve_codex_oauth_context_length(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("resolve_codex_oauth_context_length");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s resolve_codex_oauth_context_length <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
