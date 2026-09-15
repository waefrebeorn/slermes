/* gate: agent_interrupt_compat_py__request_hard_interrupt — traced copy of agent_interrupt_compat_request_hard_interrupt */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_interrupt_compat_request_hard_interrupt(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_interrupt_compat_request_hard_interrupt");
    int result = agent_interrupt_compat_request_hard_interrupt(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_interrupt_compat_request_hard_interrupt");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_interrupt_compat_request_hard_interrupt <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
