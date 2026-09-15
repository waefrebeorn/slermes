/* gate: agent_anthropic_adapter_py___supports_adaptive_thinking — traced copy of cli_agent_anthropic_adapter__supports_adaptive_thinking */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cli_agent_anthropic_adapter__supports_adaptive_thinking(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cli_agent_anthropic_adapter__supports_adaptive_thinking");
    int result = cli_agent_anthropic_adapter__supports_adaptive_thinking(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cli_agent_anthropic_adapter__supports_adaptive_thinking");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_agent_anthropic_adapter__supports_adaptive_thinking <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
