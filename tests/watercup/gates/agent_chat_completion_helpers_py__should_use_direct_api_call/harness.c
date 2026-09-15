/* gate: agent_chat_completion_helpers_py__should_use_direct_api_call — traced copy of agent_chat_completion_helpers_should_use_direct_api_call */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_chat_completion_helpers_should_use_direct_api_call(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_chat_completion_helpers_should_use_direct_api_call");
    int result = agent_chat_completion_helpers_should_use_direct_api_call(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_chat_completion_helpers_should_use_direct_api_call");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_chat_completion_helpers_should_use_direct_api_call <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
