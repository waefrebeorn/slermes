/* gate: agent_moa_trace_py___traces_enabled_and_dir — traced copy of agent_moa_trace_u_traces_enabled_and_dir */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_moa_trace_u_traces_enabled_and_dir(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_moa_trace_u_traces_enabled_and_dir");
    int result = agent_moa_trace_u_traces_enabled_and_dir(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_moa_trace_u_traces_enabled_and_dir");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_moa_trace_u_traces_enabled_and_dir <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
