/* gate: agent_anthropic_adapter_py___accepts_thinking_disable — traced copy of aa_accepts_thinking_disable */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool aa_accepts_thinking_disable(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("aa_accepts_thinking_disable");
    bool result = aa_accepts_thinking_disable(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("aa_accepts_thinking_disable");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s aa_accepts_thinking_disable <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
