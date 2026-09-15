/* gate: agent_agent_runtime_helpers_py__copy_reasoning_content_for_api — traced copy of agent_agent_runtime_helpers_copy_reasoning_content_for_api */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_agent_runtime_helpers_copy_reasoning_content_for_api(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_agent_runtime_helpers_copy_reasoning_content_for_api");
    int result = agent_agent_runtime_helpers_copy_reasoning_content_for_api(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_agent_runtime_helpers_copy_reasoning_content_for_api");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_agent_runtime_helpers_copy_reasoning_content_for_api <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
