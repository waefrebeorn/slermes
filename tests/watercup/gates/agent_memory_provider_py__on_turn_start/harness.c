/* gate: agent_memory_provider_py__on_turn_start — traced copy of agent_memory_provider_on_turn_start */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_memory_provider_on_turn_start(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_memory_provider_on_turn_start");
    int result = agent_memory_provider_on_turn_start(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_memory_provider_on_turn_start");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_memory_provider_on_turn_start <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
