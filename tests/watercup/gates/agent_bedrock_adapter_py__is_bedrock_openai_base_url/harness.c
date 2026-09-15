/* gate: agent_bedrock_adapter_py__is_bedrock_openai_base_url — traced copy of is_bedrock_openai_base_url */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool is_bedrock_openai_base_url(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("is_bedrock_openai_base_url");
    bool result = is_bedrock_openai_base_url(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("is_bedrock_openai_base_url");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s is_bedrock_openai_base_url <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
