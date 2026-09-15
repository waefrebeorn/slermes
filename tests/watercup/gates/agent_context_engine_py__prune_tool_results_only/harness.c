/* gate: agent_context_engine_py__prune_tool_results_only — traced copy of agent_context_engine_prune_tool_results_only */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_context_engine_prune_tool_results_only(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_context_engine_prune_tool_results_only");
    int result = agent_context_engine_prune_tool_results_only(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_context_engine_prune_tool_results_only");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_context_engine_prune_tool_results_only <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
