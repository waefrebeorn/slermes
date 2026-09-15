/* gate: agent_agent_init_py___relay_moa_reference_event — traced copy of agent_agent_init_u_relay_moa_reference_event */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_agent_init_u_relay_moa_reference_event(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_agent_init_u_relay_moa_reference_event");
    int result = agent_agent_init_u_relay_moa_reference_event(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_agent_init_u_relay_moa_reference_event");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_agent_init_u_relay_moa_reference_event <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
