/* gate: agent_prompt_caching_py___apply_system_cache_markers — traced copy of agent_prompt_caching_u_apply_system_cache_markers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_prompt_caching_u_apply_system_cache_markers(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_prompt_caching_u_apply_system_cache_markers");
    int result = agent_prompt_caching_u_apply_system_cache_markers(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_prompt_caching_u_apply_system_cache_markers");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_prompt_caching_u_apply_system_cache_markers <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
