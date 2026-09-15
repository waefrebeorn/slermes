/* gate: agent_tool_guardrails_py___subagent_spawn_count — traced copy of guarrd_subagent_spawn_count */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int guarrd_subagent_spawn_count(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("guarrd_subagent_spawn_count");
    int result = guarrd_subagent_spawn_count(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("guarrd_subagent_spawn_count");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s guarrd_subagent_spawn_count <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
