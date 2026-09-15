/* gate: agent_tool_guardrails_py__allows_execution — traced copy of tg_allows_execution */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool tg_allows_execution(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tg_allows_execution");
    bool result = tg_allows_execution(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tg_allows_execution");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tg_allows_execution <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
