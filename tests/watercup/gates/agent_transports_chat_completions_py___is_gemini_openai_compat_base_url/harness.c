/* gate: agent_transports_chat_completions_py___is_gemini_openai_compat_base_url — traced copy of cct_is_gemini_openai_compat_base_url */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool cct_is_gemini_openai_compat_base_url(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cct_is_gemini_openai_compat_base_url");
    bool result = cct_is_gemini_openai_compat_base_url(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cct_is_gemini_openai_compat_base_url");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cct_is_gemini_openai_compat_base_url <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
