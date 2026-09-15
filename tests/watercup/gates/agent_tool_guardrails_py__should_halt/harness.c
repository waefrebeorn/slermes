/* gate: agent_tool_guardrails_py__should_halt — traced copy of tg_should_halt */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool tg_should_halt(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tg_should_halt");
    bool result = tg_should_halt(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tg_should_halt");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tg_should_halt <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
