/* gate: agent_replay_cleanup_py__is_dangerous_confirmation — traced copy of agent_replay_cleanup_is_dangerous_confirmation */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool agent_replay_cleanup_is_dangerous_confirmation(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_replay_cleanup_is_dangerous_confirmation");
    bool result = agent_replay_cleanup_is_dangerous_confirmation(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_replay_cleanup_is_dangerous_confirmation");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_replay_cleanup_is_dangerous_confirmation <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
