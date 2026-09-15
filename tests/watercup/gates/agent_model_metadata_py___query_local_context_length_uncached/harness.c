/* gate: agent_model_metadata_py___query_local_context_length_uncached — traced copy of agent_model_metadata_u_query_local_context_length_uncached */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_model_metadata_u_query_local_context_length_uncached(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_model_metadata_u_query_local_context_length_uncached");
    int result = agent_model_metadata_u_query_local_context_length_uncached(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_model_metadata_u_query_local_context_length_uncached");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_model_metadata_u_query_local_context_length_uncached <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
