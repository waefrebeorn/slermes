/* gate: agent_anthropic_adapter_py___is_claude_model — traced copy of antr_is_claude_model */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool antr_is_claude_model(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("antr_is_claude_model");
    bool result = antr_is_claude_model(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("antr_is_claude_model");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s antr_is_claude_model <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
