/* gate: agent_model_metadata_py__grok_supports_reasoning_effort — traced copy of grok_supports_reasoning_effort */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool grok_supports_reasoning_effort(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("grok_supports_reasoning_effort");
    bool result = grok_supports_reasoning_effort(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("grok_supports_reasoning_effort");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s grok_supports_reasoning_effort <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
