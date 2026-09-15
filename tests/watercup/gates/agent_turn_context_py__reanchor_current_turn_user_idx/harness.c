/* gate: agent_turn_context_py__reanchor_current_turn_user_idx — traced copy of agent_turn_context_reanchor_current_turn_user_idx */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_turn_context_reanchor_current_turn_user_idx(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_turn_context_reanchor_current_turn_user_idx");
    int result = agent_turn_context_reanchor_current_turn_user_idx(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_turn_context_reanchor_current_turn_user_idx");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_turn_context_reanchor_current_turn_user_idx <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
