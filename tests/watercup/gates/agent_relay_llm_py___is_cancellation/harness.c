/* gate: agent_relay_llm_py___is_cancellation — traced copy of relay_llm_is_cancellation */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool relay_llm_is_cancellation(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("relay_llm_is_cancellation");
    bool result = relay_llm_is_cancellation(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("relay_llm_is_cancellation");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s relay_llm_is_cancellation <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
