/* gate: agent_chat_completion_helpers_py___is_openai_codex_backend — traced copy of cch_is_openai_codex_backend */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool cch_is_openai_codex_backend(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cch_is_openai_codex_backend");
    bool result = cch_is_openai_codex_backend(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cch_is_openai_codex_backend");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cch_is_openai_codex_backend <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
