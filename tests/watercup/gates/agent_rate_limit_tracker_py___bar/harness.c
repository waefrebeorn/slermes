/* gate: agent_rate_limit_tracker_py___bar — traced copy of agent_rate_limit_tracker_u_bar */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_rate_limit_tracker_u_bar(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_rate_limit_tracker_u_bar");
    int result = agent_rate_limit_tracker_u_bar(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_rate_limit_tracker_u_bar");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_rate_limit_tracker_u_bar <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
