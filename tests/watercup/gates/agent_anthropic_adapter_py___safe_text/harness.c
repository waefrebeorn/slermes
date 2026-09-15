/* gate: agent_anthropic_adapter_py___safe_text — traced copy of agent_anthropic_adapter_u_safe_text */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_anthropic_adapter_u_safe_text(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_anthropic_adapter_u_safe_text");
    int result = agent_anthropic_adapter_u_safe_text(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_anthropic_adapter_u_safe_text");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_anthropic_adapter_u_safe_text <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
