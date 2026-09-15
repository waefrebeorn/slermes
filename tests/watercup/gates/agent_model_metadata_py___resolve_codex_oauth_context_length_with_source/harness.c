/* gate: agent_model_metadata_py___resolve_codex_oauth_context_length_with_source — traced copy of agent_model_metadata_u_resolve_codex_oauth_context_length_wi_ce */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_model_metadata_u_resolve_codex_oauth_context_length_wi_ce(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_model_metadata_u_resolve_codex_oauth_context_length_wi_ce");
    int result = agent_model_metadata_u_resolve_codex_oauth_context_length_wi_ce(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_model_metadata_u_resolve_codex_oauth_context_length_wi_ce");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_model_metadata_u_resolve_codex_oauth_context_length_wi_ce <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
