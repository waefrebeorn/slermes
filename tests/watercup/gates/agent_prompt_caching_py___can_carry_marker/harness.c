/* gate: agent_prompt_caching_py___can_carry_marker — traced copy of agent_prompt_caching_u_can_carry_marker */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_prompt_caching_u_can_carry_marker(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_prompt_caching_u_can_carry_marker");
    int result = agent_prompt_caching_u_can_carry_marker(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_prompt_caching_u_can_carry_marker");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_prompt_caching_u_can_carry_marker <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
