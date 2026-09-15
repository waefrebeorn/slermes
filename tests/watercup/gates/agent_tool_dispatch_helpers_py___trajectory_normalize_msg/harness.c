/* gate: agent_tool_dispatch_helpers_py___trajectory_normalize_msg — traced copy of agent_tool_dispatch_helpers_u_trajectory_normalize_msg */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_tool_dispatch_helpers_u_trajectory_normalize_msg(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_tool_dispatch_helpers_u_trajectory_normalize_msg");
    int result = agent_tool_dispatch_helpers_u_trajectory_normalize_msg(input);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_tool_dispatch_helpers_u_trajectory_normalize_msg");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_tool_dispatch_helpers_u_trajectory_normalize_msg <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
