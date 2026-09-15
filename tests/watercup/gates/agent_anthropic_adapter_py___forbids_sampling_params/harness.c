/* gate: agent_anthropic_adapter_py___forbids_sampling_params — traced copy of cli_agent_anthropic_adapter__forbids_sampling_params */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cli_agent_anthropic_adapter__forbids_sampling_params(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cli_agent_anthropic_adapter__forbids_sampling_params");
    int result = cli_agent_anthropic_adapter__forbids_sampling_params(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cli_agent_anthropic_adapter__forbids_sampling_params");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_agent_anthropic_adapter__forbids_sampling_params <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
