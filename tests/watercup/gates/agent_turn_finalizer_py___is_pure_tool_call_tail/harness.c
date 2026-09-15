/* gate: agent_turn_finalizer_py___is_pure_tool_call_tail — traced copy of agent_turn_finalizer_u_is_pure_tool_call_tail */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_turn_finalizer_u_is_pure_tool_call_tail(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_turn_finalizer_u_is_pure_tool_call_tail");
    int result = agent_turn_finalizer_u_is_pure_tool_call_tail(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_turn_finalizer_u_is_pure_tool_call_tail");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_turn_finalizer_u_is_pure_tool_call_tail <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
