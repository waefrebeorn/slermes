/* gate: agent_tool_executor_py__execute_tool_calls_segmented — traced copy of agent_tool_executor_execute_tool_calls_segmented */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_tool_executor_execute_tool_calls_segmented(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_tool_executor_execute_tool_calls_segmented");
    int result = agent_tool_executor_execute_tool_calls_segmented(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_tool_executor_execute_tool_calls_segmented");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_tool_executor_execute_tool_calls_segmented <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
