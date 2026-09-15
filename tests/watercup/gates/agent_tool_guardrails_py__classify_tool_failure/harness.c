/* gate: agent_tool_guardrails_py__classify_tool_failure — traced copy of tg_classify_tool_failure */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool tg_classify_tool_failure(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tg_classify_tool_failure");
    bool result = tg_classify_tool_failure(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tg_classify_tool_failure");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tg_classify_tool_failure <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
