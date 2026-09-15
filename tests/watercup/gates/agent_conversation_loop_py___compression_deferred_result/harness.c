/* gate: agent_conversation_loop_py___compression_deferred_result — traced copy of agent_conversation_loop_u_compression_deferred_result */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_conversation_loop_u_compression_deferred_result(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_conversation_loop_u_compression_deferred_result");
    int result = agent_conversation_loop_u_compression_deferred_result(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_conversation_loop_u_compression_deferred_result");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_conversation_loop_u_compression_deferred_result <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
