/* gate: agent_bedrock_adapter_py__is_openai_bedrock_model — traced copy of is_openai_bedrock_model */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool is_openai_bedrock_model(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("is_openai_bedrock_model");
    bool result = is_openai_bedrock_model(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("is_openai_bedrock_model");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s is_openai_bedrock_model <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
